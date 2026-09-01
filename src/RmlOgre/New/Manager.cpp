#include "Manager.h"

#include "CommandBuffer/OgreCbDrawCall.h"
#include "CommandBuffer/OgreCbPipelineStateObject.h"
#include "CommandBuffer/OgreCbShaderBuffer.h"
#include "CommandBuffer/OgreCommandBuffer.h"
#include "OgreCamera.h"
#include "OgreHighLevelGpuProgramManager.h"
#include "OgreHlms.h"
#include "OgreHlmsManager.h"
#include "Renderable.h"
#include "OgreMaterialManager.h"
#include "OgrePass.h"
#include "OgreRenderQueue.h"
#include "OgreRenderSystem.h"
#include "OgreRoot.h"
#include "OgreSceneManager.h"
#include "OgreTechnique.h"
#include "OgreTextureBox.h"
#include "OgreTextureGpu.h"
#include "OgreTextureGpuManager.h"
#include "OgreUnifiedHighLevelGpuProgram.h"
#include "Vao/OgreIndirectBufferPacked.h"
#include "Vao/OgreVaoManager.h"
#include "Vao/OgreVertexArrayObject.h"

using namespace RmlOgre;

namespace
{
    Ogre::TextureGpu* BlankTexture = nullptr;

    const Ogre::HlmsCache c_dummyCache( 0, Ogre::HLMS_MAX, Ogre::HLMS_CACHE_FLAGS_NONE, Ogre::HlmsPso() );

    class ImguiDummyMO final : public Ogre::MovableObject
    {
    public:
        ImguiDummyMO( Ogre::IdType id, Ogre::ObjectMemoryManager *objectMemoryManager, Ogre::SceneManager *manager,
                      Ogre::uint8 renderQueueId ) :
            MovableObject( id, objectMemoryManager, manager, renderQueueId )
        {
        }
        ~ImguiDummyMO() override {}

        // Overrides from MovableObject
        const Ogre::String &getMovableType() const override { return Ogre::BLANKSTRING; }
    };

    void createBlankTexture()
    {
        Ogre::TextureGpuManager *textureManager = Ogre::Root::getSingleton().getRenderSystem()->getTextureGpuManager();

        BlankTexture = textureManager->createTexture(
            "RmlUi/BlankTexture", Ogre::GpuPageOutStrategy::AlwaysKeepSystemRamCopy,
            Ogre::TextureFlags::ManualTexture, Ogre::TextureTypes::Type2D );
        
        BlankTexture->setResolution( 1u, 1u );
        BlankTexture->setPixelFormat( Ogre::PixelFormatGpu::PFG_RGBA8_UNORM );

        Ogre::Image2 image;
        image.createEmptyImageLike( BlankTexture );
        Ogre::TextureBox dstBox = image.getData( 0u );
    
        uint32_t whitePixel = 0xFFFFFFFF;
        memcpy( dstBox.data, &whitePixel, sizeof( uint32_t ) );

        BlankTexture->scheduleTransitionTo( Ogre::GpuResidency::Resident );
        image.uploadTo( BlankTexture, 0u, 0u );
    }

}

//-----------------------------------------------------------------------------
Manager::Manager() :
    mIndirectBuffer( 0 ),
    mCommandBuffer( 0 ),
    mDummyMovableObject( 0 )
{
    mCommandBuffer = new Ogre::CommandBuffer();
    createPrograms();
    createBlankTexture();
}
//-----------------------------------------------------------------------------
Manager::~Manager()
{
    destroyAllResources();
    delete mDummyMovableObject;
    mDummyMovableObject = 0;
    delete mCommandBuffer;
    mCommandBuffer = 0;
}
//-----------------------------------------------------------------------------
void Manager::destroyAllResources()
{
    Ogre::VaoManager *vaoManager = Ogre::Root::getSingleton().getRenderSystem()->getVaoManager();

    for (auto* renderable : mScheduledRenderables)
    {
        renderable->destroyBuffers( vaoManager );
        delete renderable;
    }
    mScheduledRenderables.clear();

    RenderableMap::const_iterator itor = mAvailableRenderables.begin();
    RenderableMap::const_iterator endt = mAvailableRenderables.end();

    while( itor != endt )
    {
        for (auto* renderable : itor->second)
        {
            renderable->destroyBuffers( vaoManager );
            delete renderable;
        }
        const Ogre::String materialName = "!!OgreImgui_" + itor->first->getName().getReleaseText();
        Ogre::MaterialPtr material = Ogre::MaterialManager::getSingleton().getByName(
            materialName, Ogre::ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME );
        Ogre::MaterialManager::getSingleton().remove( material );
        ++itor;
    }
    mAvailableRenderables.clear();

    if( mIndirectBuffer )
    {
        if (mIndirectBuffer->getMappingState() != Ogre::MS_UNMAPPED)
            mIndirectBuffer->unmap(Ogre::UO_UNMAP_ALL);
        vaoManager->destroyIndirectBuffer( mIndirectBuffer );
        mIndirectBuffer = 0;
    }

    mCommandBuffer->clear();
}
//-----------------------------------------------------------------------------
Ogre::Matrix4 Manager::getProjectionMatrix( Ogre::RenderSystem* rs, const bool bRequiresTextureFlipping,
                                           const Ogre::Camera* currentCamera, float vpWidth, float vpHeight ) const
{
    Ogre::Matrix4 projectionMatrix( 2.0f / vpWidth, 0.0f, 0.0f, -1.0f,  //
                              0.0f, -2.0f / vpHeight, 0.0f, 1.0f,  //
                              0.0f, 0.0f, -1.0f, 0.0f,                     //
                              0.0f, 0.0f, 0.0f, 1.0f );
    // Still need to take RS depth into account.
    rs->_convertProjectionMatrix( projectionMatrix, projectionMatrix );
#if OGRE_NO_VIEWPORT_ORIENTATIONMODE == 0
    projectionMatrix =
        projectionMatrix * Quaternion( currentCamera->getOrientationModeAngle(), Vector3::UNIT_Z );
#endif

    if( bRequiresTextureFlipping )
    {
        // Invert transformed y.
        projectionMatrix[1][0] = -projectionMatrix[1][0];
        projectionMatrix[1][1] = -projectionMatrix[1][1];
        projectionMatrix[1][2] = -projectionMatrix[1][2];
        projectionMatrix[1][3] = -projectionMatrix[1][3];
    }
    return projectionMatrix;
}
//-----------------------------------------------------------------------------
void Manager::createPrograms()
{
    static const char *vertexShaderSrcD3D11 = {
        "uniform float4x4 ProjectionMatrix;\n"
        "struct VS_INPUT\n"
        "{\n"
        "float2 pos : POSITION;\n"
        "float4 col : COLOR0;\n"
        "float2 uv  : TEXCOORD0;\n"
        "};\n"
        "struct PS_INPUT\n"
        "{\n"
        "float4 pos : SV_POSITION;\n"
        "float4 col : COLOR0;\n"
        "float2 uv  : TEXCOORD0;\n"
        "};\n"
        "PS_INPUT main(VS_INPUT input)\n"
        "{\n"
        "PS_INPUT output;\n"
        "output.pos = mul(ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\n"
        "output.col = input.col;\n"
        "output.uv  = input.uv;\n"
        "return output;\n"
        "}"
    };
    static const char *pixelShaderSrcD3D11 = {
        "struct PS_INPUT\n"
        "{\n"
        "float4 pos : SV_POSITION;\n"
        "float4 col : COLOR0;\n"
        "float2 uv  : TEXCOORD0;\n"
        "};\n"
        "sampler sampler0: register(s0);\n"
        "Texture2D texture0: register(t0);\n"
        "\n"
        "float4 main(PS_INPUT input) : SV_Target\n"
        "{\n"
        "float4 out_col = input.col * texture0.Sample(sampler0, input.uv);\n"
        "return out_col; \n"
        "}"
    };

    static const char *vertexShaderSrcGLSL = {
        "#version 150\n"
        "uniform mat4 ProjectionMatrix; \n"
        "in vec2 vertex;\n"
        "in vec4 colour;\n"
        "in vec2 uv0;\n"
        "out vec2 Texcoord;\n"
        "out vec4 col;\n"
        "void main()\n"
        "{\n"
        "gl_Position = ProjectionMatrix* vec4(vertex.xy, 0.f, 1.f);\n"
        "Texcoord  = uv0;\n"
        "col = colour;\n"
        "}"
    };
    static const char *pixelShaderSrcGLSL = {
        "#version 150\n"
        "in vec2 Texcoord;\n"
        "in vec4 col;\n"
        "uniform sampler2D sampler0;\n"
        "out vec4 out_col;\n"
        "void main()\n"
        "{\n"
        "out_col = col * texture(sampler0, Texcoord);\n"
        "}"
    };
    static const char *vertexShaderSrcVK = {
        "vulkan( layout( ogre_P0 ) uniform Params { )\n"
        "    uniform mat4 ProjectionMatrix; \n"
        "vulkan( }; )\n"
        "vulkan_layout( OGRE_POSITION ) in vec2 vertex;\n"
        "vulkan_layout( OGRE_DIFFUSE ) in vec4 colour;\n"
        "vulkan_layout( OGRE_TEXCOORD0 ) in vec2 Texcoord;\n"
        "vulkan_layout( location = 1 )\n"
        "out block\n"
        "{\n"
        "    vec2 uv0;\n"
        "    vec4 col;\n"
        "} outVs;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = ProjectionMatrix* vec4(vertex.xy, 0.f, 1.f);\n"
        "    outVs.uv0 = Texcoord;\n"
        "    outVs.col = colour;\n"
        "}"
    };
    static const char *pixelShaderSrcVK = {
        "vulkan_layout( ogre_t0 ) uniform texture2D sampler0;\n"
        "vulkan( layout( ogre_s0 ) uniform sampler texSampler );\n"
        "vulkan_layout( location = 0 )\n"
        "out vec4 out_col;\n"
        "vulkan_layout( location = 1 )\n"
        "in block\n"
        "{\n"
        "    vec2 uv0;\n"
        "    vec4 col;\n"
        "} inPs;\n"
        "void main()\n"
        "{\n"
        "    out_col = inPs.col * texture( vkSampler2D( sampler0, texSampler ), inPs.uv0 );"
        "}"
    };
    static const char *fragmentShaderSrcMetal = {
        "#include <metal_stdlib>\n"
        "using namespace metal;\n"
        "\n"
        "struct VertexOut {\n"
        "    float4 position [[position]];\n"
        "    float4 colour;\n"
        "    float2 texCoords;\n"
        "};\n"
        "\n"
        "fragment float4 main_metal(VertexOut in [[stage_in]],\n"
        "                             texture2d<float> texture [[texture(0)]]) {\n"
        "    constexpr sampler linearSampler(coord::normalized, min_filter::linear, mag_filter::linear, "
        "mip_filter::linear);\n"
        "    float4 texColour = texture.sample(linearSampler, in.texCoords);\n"
        "    return in.colour * texColour;\n"
        "}\n"
    };

    static const char *vertexShaderSrcMetal = {
        "#include <metal_stdlib>\n"
        "using namespace metal;\n"
        "\n"
        "struct Constant {\n"
        "    float4x4 ProjectionMatrix;\n"
        "};\n"
        "\n"
        "struct VertexIn {\n"
        "    float2 position  [[attribute(VES_POSITION)]];\n"
        "    float4 colour     [[attribute(VES_DIFFUSE)]];\n"
        "    float2 texCoords [[attribute(VES_TEXTURE_COORDINATES0)]];\n"
        "};\n"
        "\n"
        "struct VertexOut {\n"
        "    float4 position [[position]];\n"
        "    float2 texCoords;\n"
        "    float4 colour;\n"
        "};\n"
        "\n"
        "vertex VertexOut vertex_main(VertexIn in                 [[stage_in]],\n"
        "                             constant Constant &uniforms [[buffer(PARAMETER_SLOT)]]) {\n"
        "    VertexOut out;\n"
        "    out.position = uniforms.ProjectionMatrix * float4(in.position, 0, 1);\n"

        "    out.texCoords = in.texCoords;\n"
        "    out.colour = in.colour;\n"

        "    return out;\n"
        "}\n"
    };

    // create the default shadows material
    Ogre::HighLevelGpuProgramManager &mgr = Ogre::HighLevelGpuProgramManager::getSingleton();

    Ogre::HighLevelGpuProgramPtr vertexShaderUnified = mgr.getByName( "imgui/VP" );
    Ogre::HighLevelGpuProgramPtr pixelShaderUnified = mgr.getByName( "imgui/FP" );

    Ogre::HighLevelGpuProgramPtr vertexShaderD3D11 = mgr.getByName( "imgui/VP/D3D11" );
    Ogre::HighLevelGpuProgramPtr pixelShaderD3D11 = mgr.getByName( "imgui/FP/D3D11" );

    Ogre::HighLevelGpuProgramPtr vertexShaderGL = mgr.getByName( "imgui/VP/GL150" );
    Ogre::HighLevelGpuProgramPtr pixelShaderGL = mgr.getByName( "imgui/FP/GL150" );

    Ogre::HighLevelGpuProgramPtr vertexShaderVK = mgr.getByName( "imgui/VP/VK" );
    Ogre::HighLevelGpuProgramPtr pixelShaderVK = mgr.getByName( "imgui/FP/VK" );

    Ogre::HighLevelGpuProgramPtr vertexShaderMetal = mgr.getByName( "imgui/VP/Metal" );
    Ogre::HighLevelGpuProgramPtr pixelShaderMetal = mgr.getByName( "imgui/FP/Metal" );

    if( !vertexShaderUnified )
    {
        vertexShaderUnified =
            mgr.createProgram( "imgui/VP", Ogre::ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME, "unified",
                               Ogre::GPT_VERTEX_PROGRAM );
    }
    if( !pixelShaderUnified )
    {
        pixelShaderUnified =
            mgr.createProgram( "imgui/FP", Ogre::ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME, "unified",
                               Ogre::GPT_FRAGMENT_PROGRAM );
    }

    Ogre::UnifiedHighLevelGpuProgram *vertexShaderPtr =
        static_cast<Ogre::UnifiedHighLevelGpuProgram *>( vertexShaderUnified.get() );
    Ogre::UnifiedHighLevelGpuProgram *pixelShaderPtr =
        static_cast<Ogre::UnifiedHighLevelGpuProgram *>( pixelShaderUnified.get() );

    if( !vertexShaderD3D11 )
    {
        vertexShaderD3D11 =
            mgr.createProgram( "imgui/VP/D3D11", Ogre::ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME,
                               "hlsl", Ogre::GPT_VERTEX_PROGRAM );
        vertexShaderD3D11->setParameter( "target", "vs_5_0 vs_4_0" );
        vertexShaderD3D11->setParameter( "entry_point", "main" );
        vertexShaderD3D11->setSource( vertexShaderSrcD3D11 );
        vertexShaderD3D11->load();

        vertexShaderPtr->addDelegateProgram( vertexShaderD3D11->getName() );
    }
    if( !pixelShaderD3D11 )
    {
        pixelShaderD3D11 =
            mgr.createProgram( "imgui/FP/D3D11", Ogre::ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME,
                               "hlsl", Ogre::GPT_FRAGMENT_PROGRAM );
        pixelShaderD3D11->setParameter( "target", "ps_5_0 ps_4_0" );
        pixelShaderD3D11->setParameter( "entry_point", "main" );
        pixelShaderD3D11->setSource( pixelShaderSrcD3D11 );
        pixelShaderD3D11->load();

        pixelShaderPtr->addDelegateProgram( pixelShaderD3D11->getName() );
    }

    if( !vertexShaderMetal )
    {
        vertexShaderMetal =
            mgr.createProgram( "imgui/VP/Metal", Ogre::ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME,
                               "metal", Ogre::GPT_VERTEX_PROGRAM );
        vertexShaderMetal->setParameter( "entry_point", "vertex_main" );
        vertexShaderMetal->setSource( vertexShaderSrcMetal );
        vertexShaderMetal->load();
        vertexShaderPtr->addDelegateProgram( vertexShaderMetal->getName() );
    }
    if( !pixelShaderMetal )
    {
        pixelShaderMetal =
            mgr.createProgram( "imgui/FP/Metal", Ogre::ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME,
                               "metal", Ogre::GPT_FRAGMENT_PROGRAM );
        vertexShaderMetal->setParameter( "entry_point", "fragment_main" );
        pixelShaderMetal->setSource( fragmentShaderSrcMetal );
        pixelShaderMetal->load();
        pixelShaderPtr->addDelegateProgram( pixelShaderMetal->getName() );
    }

    if( !vertexShaderGL )
    {
        vertexShaderGL =
            mgr.createProgram( "imgui/VP/GL150", Ogre::ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME,
                               "glsl", Ogre::GPT_VERTEX_PROGRAM );
        vertexShaderGL->setSource( vertexShaderSrcGLSL );
        vertexShaderGL->load();
        vertexShaderPtr->addDelegateProgram( vertexShaderGL->getName() );
    }
    if( !pixelShaderGL )
    {
        pixelShaderGL =
            mgr.createProgram( "imgui/FP/GL150", Ogre::ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME,
                               "glsl", Ogre::GPT_FRAGMENT_PROGRAM );
        pixelShaderGL->setSource( pixelShaderSrcGLSL );
        pixelShaderGL->load();

        pixelShaderPtr->addDelegateProgram( pixelShaderGL->getName() );
    }

    if( !vertexShaderVK )
    {
        vertexShaderVK =
            mgr.createProgram( "imgui/VP/vulkan", Ogre::ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME,
                               "glslvk", Ogre::GPT_VERTEX_PROGRAM );
        vertexShaderVK->setSource( vertexShaderSrcVK );
        vertexShaderVK->setPrefabRootLayout( Ogre::PrefabRootLayout::Standard );
        vertexShaderVK->load();
        vertexShaderPtr->addDelegateProgram( vertexShaderVK->getName() );
    }
    if( !pixelShaderVK )
    {
        pixelShaderVK =
            mgr.createProgram( "imgui/FP/vulkan", Ogre::ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME,
                               "glslvk", Ogre::GPT_FRAGMENT_PROGRAM );
        pixelShaderVK->setSource( pixelShaderSrcVK );
        pixelShaderVK->setPrefabRootLayout( Ogre::PrefabRootLayout::Standard );
        pixelShaderVK->load();

        pixelShaderPtr->addDelegateProgram( pixelShaderVK->getName() );
    }
}
//-----------------------------------------------------------------------------
Ogre::MaterialPtr Manager::createMaterialFor( Ogre::TextureGpu* texture )
{
    if( !texture )
        texture = BlankTexture;

    const Ogre::String materialName = "!!OgreRmlUi_" + texture->getName().getReleaseText();
    
    Ogre::MaterialPtr rmlMaterial = Ogre::MaterialManager::getSingleton().getByName(
        materialName, Ogre::ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME );

    if( rmlMaterial )
        return rmlMaterial;

    rmlMaterial = Ogre::MaterialManager::getSingleton().create(
        materialName, Ogre::ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME );

    Ogre::HlmsBlendblock blendblock;
    blendblock.mSourceBlendFactor = Ogre::SBF_SOURCE_ALPHA;
    blendblock.mDestBlendFactor = Ogre::SBF_ONE_MINUS_SOURCE_ALPHA;
    blendblock.mSourceBlendFactorAlpha = Ogre::SBF_ONE;
    blendblock.mDestBlendFactorAlpha = Ogre::SBF_ONE_MINUS_SOURCE_ALPHA;
    blendblock.mBlendOperation = Ogre::SBO_ADD;
    blendblock.mBlendOperationAlpha = Ogre::SBO_ADD;
    blendblock.mSeparateBlend = true;
    blendblock.mIsTransparent = true;

    Ogre::HlmsMacroblock macroblock;
    macroblock.mCullMode = Ogre::CULL_NONE;
    macroblock.mDepthFunc = Ogre::CMPF_ALWAYS_PASS;
    macroblock.mDepthCheck = false;
    macroblock.mDepthWrite = false;
    macroblock.mScissorTestEnabled = true;

    Ogre::Pass *pass = rmlMaterial->getTechnique( 0 )->getPass( 0 );
    pass->setFragmentProgram( "imgui/FP" );
    pass->setVertexProgram( "imgui/VP" );

    pass->setBlendblock( blendblock );
    pass->setMacroblock( macroblock );

    pass->createTextureUnitState()->setTexture( texture );

    return rmlMaterial;
}
//-----------------------------------------------------------------------------
Ogre::Renderable *Manager::getAvailableRenderable( Ogre::TextureGpu* texture )
{
    Renderable *retVal = 0;

    RenderableMap::iterator itor = mAvailableRenderables.find( texture );
    if( itor != mAvailableRenderables.end() )
    {
        if( itor->second.empty() )
        {
            auto *renderable = new Renderable();
            renderable->setMaterial( createMaterialFor( texture ) );
            itor->second.push_back( renderable );
        }
        retVal = itor->second.back();
        itor->second.pop_back();
    }
    else
    {
        auto *renderable = new Renderable();
        renderable->setMaterial( createMaterialFor( texture ) );
        mAvailableRenderables.insert( { texture, {} } );
        retVal = renderable;
    }

    mScheduledRenderables.push_back( retVal );

    return retVal;
}
//-----------------------------------------------------------------------------
//void Manager::prepareForRender( Ogre::SceneManager *sceneManager )
//{
//    // Tell ImGui to create the buffers.
//    ImGui::Render();
//
//    const ImDrawData *drawData = ImGui::GetDrawData();
//    mDrawData = drawData;
//
//    for( Ogre::Renderable *renderable : mScheduledRenderables )
//    {
//        Ogre::TextureGpu* texture = renderable->getMaterial()
//                                  ->getTechnique( 0u )
//                                  ->getPass( 0u )
//                                  ->getTextureUnitState( 0u )
//                                  ->_getTexturePtr( 0u );
//        mAvailableRenderables[texture].push_back( renderable );
//    }
//    mScheduledRenderables.clear();
//
//    size_t numNeededDraws = 0u;
//    for( int n = 0; n < drawData->CmdListsCount; n++ )
//    {
//        const ImDrawList *drawList = drawData->CmdLists[n];
//        numNeededDraws += size_t( drawList->CmdBuffer.Size );
//    }
//
//    Ogre::VaoManager *vaoManager = sceneManager->getDestinationRenderSystem()->getVaoManager();
//    if( !mDummyMovableObject )
//    {
//        mDummyMovableObject = OGRE_NEW ImguiDummyMO(
//            Id::generateNewId<Ogre::MovableObject>(),
//            &sceneManager->_getEntityMemoryManager( Ogre::SCENE_STATIC ), sceneManager, 254u );
//        sceneManager->getRootSceneNode( Ogre::SCENE_STATIC )->attachObject( mDummyMovableObject );
//        mDummyMovableObject->setVisible( false );
//        mDummyMovableObject->setCastShadows( false );
//    }
//
//    const bool supportsIndirectBuffers = vaoManager->supportsIndirectBuffers();
//
//    unsigned char *indirectDraw = 0;
//    if( numNeededDraws > 0u )
//    {
//        if( !mIndirectBuffer ||
//            ( numNeededDraws * sizeof( CbDrawIndexed ) ) > mIndirectBuffer->getNumElements() )
//        {
//            if( mIndirectBuffer )
//            {
//                if( mIndirectBuffer->getMappingState() != MS_UNMAPPED )
//                    mIndirectBuffer->unmap( UO_UNMAP_ALL );
//                vaoManager->destroyIndirectBuffer( mIndirectBuffer );
//            }
//            mIndirectBuffer = vaoManager->createIndirectBuffer( numNeededDraws * sizeof( CbDrawIndexed ),
//                                                                BT_DYNAMIC_PERSISTENT, 0, false );
//        }
//
//        if( supportsIndirectBuffers )
//        {
//            indirectDraw = static_cast<unsigned char *>(
//                mIndirectBuffer->map( 0, mIndirectBuffer->getNumElements() ) );
//        }
//        else
//        {
//            indirectDraw = mIndirectBuffer->getSwBufferPtr();
//        }
//    }
//
//    // iterate through all lists (at the moment every window has its own)
//    for( int n = 0; n < drawData->CmdListsCount; n++ )
//    {
//        const ImDrawList *drawList = drawData->CmdLists[n];
//        const ImDrawVert *vtxBuf = drawList->VtxBuffer.Data;
//        const ImDrawIdx *idxBuf = drawList->IdxBuffer.Data;
//
//        unsigned int startIdx = 0;
//
//        for( int i = 0; i < drawList->CmdBuffer.Size; i++ )
//        {
//            const ImDrawCmd *drawCmd = &drawList->CmdBuffer[i];
//
//            Ogre::TextureGpu* texture = reinterpret_cast<Ogre::TextureGpu* >( drawCmd->GetTexID() );
//            Ogre::Renderable *renderable = getAvailableRenderable( texture );
//
//            // update their vertex buffers.
//            renderable->updateVertexData( vtxBuf, &idxBuf[startIdx],
//                                          (unsigned int)drawList->VtxBuffer.Size, drawCmd->ElemCount,
//                                          vaoManager );
//
//            VertexArrayObject *vao = renderable->getVaos( VpNormal ).back();
//
//            CbDrawIndexed *cmd = reinterpret_cast<CbDrawIndexed *>( indirectDraw );
//            indirectDraw += sizeof( CbDrawIndexed );
//            cmd->primCount = vao->getPrimitiveCount();
//            cmd->instanceCount = 1u;
//            cmd->firstVertexIndex =
//                uint32_t( vao->getIndexBuffer()->_getFinalBufferStart() + vao->getPrimitiveStart() );
//            cmd->baseVertex = uint32_t( vao->getBaseVertexBuffer()->_getFinalBufferStart() );
//            cmd->baseInstance = 0u;
//
//            startIdx += drawCmd->ElemCount;
//        }
//    }
//
//    if( indirectDraw && supportsIndirectBuffers )
//        mIndirectBuffer->unmap( UO_KEEP_PERSISTENT );
//
//    // Delete unused renderables, but leaving just one element.
//    RenderableMap::iterator itor = mAvailableRenderables.begin();
//    RenderableMap::iterator endt = mAvailableRenderables.end();
//
//    while( itor != endt )
//    {
//        while( itor->second.size() > 1u )
//        {
//            Ogre::Renderable *renderable = itor->second.back();
//            renderable->destroyBuffers( vaoManager );
//            delete renderable;
//            itor->second.pop_back();
//        }
//        ++itor;
//    }
//}
//-----------------------------------------------------------------------------
void Manager::drawIntoCompositor( Ogre::RenderPassDescriptor* renderPassDesc,
                                       Ogre::TextureGpu* anyTargetTexture, Ogre::SceneManager *sceneManager,
                                       const Ogre::Camera* currentCamera )
{
    Ogre::HlmsManager *hlmsManager = Ogre::Root::getSingleton().getHlmsManager();
    Ogre::Hlms *hlms = hlmsManager->getHlms( Ogre::HLMS_LOW_LEVEL );

    Ogre::RenderSystem *renderSystem = sceneManager->getDestinationRenderSystem();
    mCommandBuffer->setCurrentRenderSystem( renderSystem );

    const bool bWasReadyForPresent = renderPassDesc->mReadyWindowForPresent;
    const Ogre::VaoManager *vaoManager = renderSystem->getVaoManager();

    int baseInstanceAndIndirectBuffers = 0;
    if( vaoManager->supportsBaseInstance() )
        baseInstanceAndIndirectBuffers = 1;

    Ogre::HlmsCache passCache = hlms->preparePassHash( 0, false, false, sceneManager );

    const int vpWidth = int( anyTargetTexture->getWidth() );
    const int vpHeight = int( anyTargetTexture->getHeight() );
    const Ogre::Vector4 viewportSize( 0, 0, 1, 1 );

    const Ogre::Matrix4 projMatrix =
        getProjectionMatrix( renderSystem, renderPassDesc->requiresTextureFlipping(), currentCamera, float(vpWidth), float(vpHeight));

    Ogre::RenderingMetrics stats;

    size_t cmdIndex = 0;
    const size_t numCmds = mDrawCmds.size();

    for (auto& cmd : mDrawCmds)
    {
        auto* renderable = cmd.renderable;
        if (!renderable)
            continue;

        Ogre::Vector4 scissors = viewportSize;
        if (cmd.scissorEnabled)
        {
            int scLeft = Ogre::Math::Clamp( static_cast<int>(cmd.scissor.x), 0, vpWidth );
            int scTop = Ogre::Math::Clamp( static_cast<int>(cmd.scissor.y), 0, vpHeight );
            int scRight = Ogre::Math::Clamp( static_cast<int>(cmd.scissor.z), 0, vpWidth );
            int scBottom = Ogre::Math::Clamp( static_cast<int>(cmd.scissor.w), 0, vpHeight );

            const float left = (float)scLeft / (float)vpWidth;
            const float top = (float)scTop / (float)vpHeight;
            const float width = (float)( scRight - scLeft ) / (float)vpWidth;
            const float height = (float)( scBottom - scTop ) / (float)vpHeight;

            scissors = Ogre::Vector4( left, top, width, height );
        }

        if( bWasReadyForPresent )
        {
            const bool bShouldBeReadyForPresent = (cmdIndex + 1) == numCmds;
            if( bShouldBeReadyForPresent != renderPassDesc->mReadyWindowForPresent )
            {
                renderSystem->endRenderPassDescriptor();
                renderPassDesc->mReadyWindowForPresent = bShouldBeReadyForPresent;
                renderPassDesc->entriesModified( Ogre::RenderPassDescriptor::Colour );
            }
        }

        renderSystem->beginRenderPassDescriptor( renderPassDesc, anyTargetTexture, 0u, &viewportSize,
                                                 &scissors, 1u, false, false );
        renderSystem->executeRenderPassDescriptorDelayedActions();

        if( cmd.texture )
        {
            renderable->getMaterial()->getTechnique(0u)->getPass(0u)
                      ->getTextureUnitState(0u)->setTexture( cmd.texture );
        }

        Ogre::Matrix4 finalProjMatrix = projMatrix * cmd.transform;
        renderable->getMaterial()->getTechnique( 0u )->getPass( 0u )
            ->getVertexProgramParameters()->setNamedConstant( "ProjectionMatrix", finalProjMatrix );

        Ogre::QueuedRenderable queuedRenderable( 0u, renderable, mDummyMovableObject );
        const Ogre::HlmsCache *hlmsCache =
            hlms->getMaterial( &c_dummyCache, passCache, queuedRenderable, false, nullptr );

        Ogre::CbPipelineStateObject *psoCmd = mCommandBuffer->addCommand<Ogre::CbPipelineStateObject>();
        *psoCmd = Ogre::CbPipelineStateObject( &hlmsCache->pso );

        hlms->fillBuffersForV2( hlmsCache, queuedRenderable, false, 0u, mCommandBuffer );

        Ogre::VertexArrayObject *vao = renderable->getVaos( Ogre::VpNormal ).back();

        *mCommandBuffer->addCommand<Ogre::CbVao>() = Ogre::CbVao( vao );
        
        // Обратите внимание: CbIndirectBuffer удален. Мы используем прямое обращение к VAO
        Ogre::CbDrawCallIndexed *drawCall = mCommandBuffer->addCommand<Ogre::CbDrawCallIndexed>();
        *drawCall = Ogre::CbDrawCallIndexed( baseInstanceAndIndirectBuffers, vao, nullptr ); 
        drawCall->numDraws = 1u;
        
        stats.mDrawCount += 1u;
        stats.mInstanceCount += 1u;
        stats.mFaceCount += vao->getPrimitiveCount() / 3u;
        stats.mVertexCount += vao->getPrimitiveCount();

        hlms->preCommandBufferExecution( mCommandBuffer );
        mCommandBuffer->execute();
        hlms->postCommandBufferExecution( mCommandBuffer );

        cmdIndex++;
    }

    renderSystem->_addMetrics( stats );

    // There was nothing for RmlUi to draw. We must still prepare the window for presenting.
    if( bWasReadyForPresent && !stats.mDrawCount )
    {
        Ogre::Vector4 scissors( 0, 0, 1, 1 );
        renderSystem->beginRenderPassDescriptor( renderPassDesc, anyTargetTexture, 0u, &viewportSize,
                                                 &scissors, 1u, false, false );
        renderSystem->executeRenderPassDescriptorDelayedActions();
    }
}

void Manager::BeginFrame()
{

}

 void Manager::EndFrame()
 {
    mDrawCmds.clear();
 }

Rml::CompiledGeometryHandle Manager::CompileGeometry(
    Rml::Span<const Rml::Vertex> vertices,
    Rml::Span<const int> indices)
{
    Ogre::VaoManager* vaoManager = Ogre::Root::getSingleton().getRenderSystem()->getVaoManager();
    auto* renderable = new Renderable();
    renderable->updateVertexData(
        vertices.data(), 
        indices.data(), 
        vertices.size(), 
        indices.size(), 
        vaoManager
    );

    return reinterpret_cast<Rml::CompiledGeometryHandle>(renderable);
}

void Manager::ReleaseGeometry(Rml::CompiledGeometryHandle geometry)
{
    auto* renderable = reinterpret_cast<Renderable*>(geometry);
    if (renderable)
    {
        auto* vaoManager = Ogre::Root::getSingleton().getRenderSystem()->getVaoManager();
        renderable->destroyBuffers(vaoManager);
        delete renderable;
    }
}

void Manager::RenderGeometry(
        Rml::CompiledGeometryHandle geometry,
        Rml::Vector2f translation,
        Rml::TextureHandle texture) 
{

    auto& cmd = mDrawCmds.emplace_back();
    cmd.renderable = reinterpret_cast<Renderable*>(geometry);
    cmd.texture = reinterpret_cast<Ogre::TextureGpu*>(texture);
    cmd.scissor = mCurrentScissor;
    cmd.scissorEnabled = mScissorEnabled;
    
    cmd.transform = mCurrentTransform;
    cmd.transform.setTrans(Ogre::Vector3(translation.x, translation.y, 0));

    if (!cmd.renderable->getMaterial())
        cmd.renderable->setMaterial(createMaterialFor(cmd.texture));
}

// Управление ножницами
void Manager::EnableScissorRegion(bool enable) {
    mScissorEnabled = enable;
}

void Manager::SetScissorRegion(Rml::Rectanglei region) {
    mCurrentScissor = Ogre::Vector4(region.Left(), region.Top(), region.Right(), region.Bottom());
}

void Manager::SetTransform(const Rml::Matrix4f* transform) {
    if (transform)
        mCurrentTransform = Ogre::Matrix4(transform->data());
    else
        mCurrentTransform = Ogre::Matrix4::IDENTITY;
}

Rml::TextureHandle Manager::LoadTexture(
    Rml::Vector2i& texture_dimensions,
    const Rml::String& source
)
{
    return 0;
}

Rml::TextureHandle Manager::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions)
{
    Ogre::TextureGpuManager *textureManager = Ogre::Root::getSingleton().getRenderSystem()->getTextureGpuManager();
    
    Ogre::String texName = "RmlUiTex_" + Ogre::StringConverter::toString(Ogre::Id::generateNewId<Ogre::TextureGpu>());
    Ogre::TextureGpu* tex = textureManager->createTexture(
        texName, Ogre::GpuPageOutStrategy::AlwaysKeepSystemRamCopy,
        Ogre::TextureFlags::ManualTexture, Ogre::TextureTypes::Type2D);
        
    tex->setResolution(source_dimensions.x, source_dimensions.y);
    tex->setPixelFormat(Ogre::PixelFormatGpu::PFG_RGBA8_UNORM_SRGB);
    
    Ogre::Image2 image;
    image.createEmptyImageLike(tex);
    Ogre::TextureBox dstBox = image.getData(0u);
    
    const Rml::byte* srcData = source.data();
    for(uint32_t y = 0u; y < dstBox.height; ++y) {
        void *dstRaw = dstBox.at(0u, y, 0u);
        memcpy(dstRaw, &srcData[y * dstBox.width * 4u], dstBox.width * 4u);
    }
    
    tex->scheduleTransitionTo(Ogre::GpuResidency::Resident);
    image.uploadTo(tex, 0u, tex->getNumMipmaps() - 1u);
    
    return reinterpret_cast<Rml::TextureHandle>(tex);
}

void Manager::ReleaseTexture(Rml::TextureHandle texture_handle)
{
    Ogre::TextureGpu* tex = reinterpret_cast<Ogre::TextureGpu*>(texture_handle);
    if (tex)
    {
        Ogre::TextureGpuManager *textureManager = Ogre::Root::getSingleton().getRenderSystem()->getTextureGpuManager();
        textureManager->destroyTexture(tex);
    }
}
