#include "Manager.h"

#include "CommandBuffer/OgreCbDrawCall.h"
#include "CommandBuffer/OgreCbPipelineStateObject.h"
#include "CommandBuffer/OgreCbShaderBuffer.h"
#include "CommandBuffer/OgreCommandBuffer.h"
#include "OgreCamera.h"
#include "OgreHighLevelGpuProgramManager.h"
#include "OgreHlms.h"
#include "OgreHlmsUnlit.h"
#include "OgreHlmsManager.h"
#include "OgreHlmsUnlitDatablock.h"
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

    Ogre::HlmsUnlitDatablock* BlankDatablock = nullptr;
    Ogre::MaterialPtr         BlankMaterial;

    const Ogre::HlmsCache c_dummyCache( 0, Ogre::HLMS_MAX, Ogre::HLMS_CACHE_FLAGS_NONE, Ogre::HlmsPso() );

    class ImguiDummyMO final : public Ogre::MovableObject
    {
    public:
        ImguiDummyMO( Ogre::IdType id, Ogre::ObjectMemoryManager *objectMemoryManager, Ogre::SceneManager *manager,
                      Ogre::uint8 renderQueueId ) :
            MovableObject( id, objectMemoryManager, manager, renderQueueId )
        {
        }
        ~ImguiDummyMO() final {}

        // Overrides from MovableObject
        const Ogre::String &getMovableType() const final { return Ogre::BLANKSTRING; }
    };

    void createBlankDatablock(Ogre::HlmsMacroblock& macroblock, Ogre::HlmsBlendblock& blendblock)
    {
        auto hlms = static_cast<Ogre::HlmsUnlit*>(Ogre::Root::getSingleton().getHlmsManager()->getHlms(Ogre::HLMS_UNLIT));
		BlankDatablock = static_cast<Ogre::HlmsUnlitDatablock*>(hlms->createDatablock(
			"NoTexture",
			"NoTexture",
			macroblock,
			blendblock,
			Ogre::HlmsParamVec())
		);
	    BlankDatablock->setUseColour(true);
    }

    void createBlankTexture()
    {
        Ogre::TextureGpuManager *textureManager = Ogre::Root::getSingleton().getRenderSystem()->getTextureGpuManager();

        BlankTexture = textureManager->createTexture("RmlUi/BlankTexture",
            Ogre::GpuPageOutStrategy::AlwaysKeepSystemRamCopy,
            0,
            Ogre::TextureTypes::Type2D,
            Ogre::BLANKSTRING);
        BlankTexture->setNumMipmaps(1);
        BlankTexture->setResolution( 1u, 1u );
        BlankTexture->setPixelFormat( Ogre::PixelFormatGpu::PFG_RGBA8_UNORM );

        auto image = OGRE_NEW Ogre::Image2;
        image->createEmptyImageLike( BlankTexture );
        Ogre::TextureBox dstBox = image->getData( 0u );
    
        uint32_t whitePixel = 0xFFFFFFFF;
        memcpy( dstBox.data, &whitePixel, sizeof( uint32_t ) );

        BlankTexture->scheduleTransitionTo(Ogre::GpuResidency::Resident, image);

        //// Tweak via _setAutoDelete so the internal data is copied as a pointer
        //// instead of performing a deep copy of the data; while leaving the responsability
        //// of freeing memory to imagePtr instead.
        //image._setAutoDelete( false );
        //auto imagePtr = new Ogre::Image2( image );
        //imagePtr->_setAutoDelete( true );

        //if (BlankTexture->getNextResidencyStatus() == Ogre::GpuResidency::Resident)
        //    BlankTexture->scheduleTransitionTo(Ogre::GpuResidency::OnStorage);
        //// Ogre will call "delete imagePtr" when done, because we're passing
        //// true to autoDeleteImage argument in scheduleTransitionTo.
        //BlankTexture->scheduleTransitionTo(Ogre::GpuResidency::Resident, imagePtr, true);
    }

    void createBlankMaterial(Ogre::HlmsMacroblock& macroblock, Ogre::HlmsBlendblock& blendblock)
    {
        const Ogre::String materialName = "!!OgreRmlUi_BlankTexture";
    
        BlankMaterial = Ogre::MaterialManager::getSingleton().create(
            materialName, Ogre::ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME );

        Ogre::Pass *pass = BlankMaterial->getTechnique( 0 )->getPass( 0 );
        pass->setFragmentProgram( "imgui/FP" );
        pass->setVertexProgram( "imgui/VP" );

        pass->setBlendblock( blendblock );
        pass->setMacroblock( macroblock );

        auto textureUnit = pass->createTextureUnitState();
        textureUnit->setTexture( BlankTexture );
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

    m_samplerblock.setFiltering(Ogre::TFO_NONE);
    m_samplerblock.setAddressingMode(Ogre::TAM_CLAMP);

	m_blendblock.mBlendOperation = Ogre::SceneBlendOperation::SBO_ADD;
	m_blendblock.mDestBlendFactor = Ogre::SceneBlendFactor::SBF_ONE_MINUS_SOURCE_ALPHA;
	m_blendblock.mSourceBlendFactor = Ogre::SceneBlendFactor::SBF_ONE;

	m_macroblock.mScissorTestEnabled = true;
	m_macroblock.mDepthCheck = true;
	m_macroblock.mDepthWrite = true;
	m_macroblock.mCullMode = Ogre::CULL_NONE;

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
        "gl_Position = ProjectionMatrix * vec4(vertex.xy, 0.f, 1.f);\n"
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
    if (!texture)
        texture = BlankTexture;

    const Ogre::String materialName = "!!OgreRmlUi_" + texture->getName().getReleaseText();
    
    Ogre::MaterialPtr rmlMaterial = Ogre::MaterialManager::getSingleton().getByName(
        materialName, Ogre::ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME );

    if( rmlMaterial )
        return rmlMaterial;

    rmlMaterial = Ogre::MaterialManager::getSingleton().create(
        materialName, Ogre::ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME );

    Ogre::Pass *pass = rmlMaterial->getTechnique( 0 )->getPass( 0 );
    pass->setFragmentProgram( "imgui/FP" );
    pass->setVertexProgram( "imgui/VP" );

    pass->setSamplerblock(m_samplerblock);
    pass->setBlendblock( m_blendblock );
    pass->setMacroblock( m_macroblock );

    auto textureUnit = pass->createTextureUnitState();
    textureUnit->setTexture( texture );

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

    Ogre::RenderSystem *renderSystem = sceneManager->getDestinationRenderSystem();
    Ogre::VaoManager *vaoManager = renderSystem->getVaoManager();
    const bool supportsIndirectBuffers = vaoManager->supportsIndirectBuffers();
    const size_t numNeededDraws = mDrawCmds.size();

    const bool bWasReadyForPresent = renderPassDesc->mReadyWindowForPresent;
    const Ogre::Vector4 viewportSize( 0, 0, 1, 1 );
    Ogre::RenderingMetrics stats;

    unsigned char *indirectDraw = 0;
    if( numNeededDraws > 0u )
    {
        if( !mIndirectBuffer || ( numNeededDraws * sizeof( Ogre::CbDrawIndexed ) ) > mIndirectBuffer->getNumElements() )
        {
            if( mIndirectBuffer )
            {
                if( mIndirectBuffer->getMappingState() != Ogre::MS_UNMAPPED )
                    mIndirectBuffer->unmap( Ogre::UO_UNMAP_ALL );
                vaoManager->destroyIndirectBuffer( mIndirectBuffer );
            }
            mIndirectBuffer = vaoManager->createIndirectBuffer( numNeededDraws * sizeof( Ogre::CbDrawIndexed ),
                                                                Ogre::BT_DYNAMIC_PERSISTENT, 0, false );
        }

        if( supportsIndirectBuffers )
            indirectDraw = static_cast<unsigned char *>( mIndirectBuffer->map( 0, mIndirectBuffer->getNumElements() ) );
        else
            indirectDraw = mIndirectBuffer->getSwBufferPtr();

        // Заполняем CbDrawIndexed для каждой команды RmlUi
        for( size_t i = 0; i < numNeededDraws; ++i )
        {
            auto* renderable = mDrawCmds[i].renderable;
            assert(renderable);
            Ogre::VertexArrayObject *vao = mDrawCmds[i].renderable->getVaos( Ogre::VpNormal ).back();

            Ogre::CbDrawIndexed *cmd = reinterpret_cast<Ogre::CbDrawIndexed *>( indirectDraw );
            indirectDraw += sizeof( Ogre::CbDrawIndexed );
            
            cmd->primCount = vao->getPrimitiveCount();
            cmd->instanceCount = 1u;
            cmd->firstVertexIndex = uint32_t( vao->getIndexBuffer()->_getFinalBufferStart() + vao->getPrimitiveStart() );
            cmd->baseVertex = uint32_t( vao->getBaseVertexBuffer()->_getFinalBufferStart() );
            cmd->baseInstance = 0u;
        }

        if( supportsIndirectBuffers )
            mIndirectBuffer->unmap( Ogre::UO_KEEP_PERSISTENT );
    }

    Ogre::HlmsManager *hlmsManager = Ogre::Root::getSingleton().getHlmsManager();
    Ogre::Hlms *hlms = hlmsManager->getHlms( Ogre::HLMS_LOW_LEVEL );

    mCommandBuffer->setCurrentRenderSystem( renderSystem );

    int baseInstanceAndIndirectBuffers = 0;
    if( vaoManager->supportsIndirectBuffers() )
        baseInstanceAndIndirectBuffers = 2;
    else if( vaoManager->supportsBaseInstance() )
        baseInstanceAndIndirectBuffers = 1;

    Ogre::HlmsCache passCache = hlms->preparePassHash( 0, false, false, sceneManager );

    const int vpWidth = int( anyTargetTexture->getWidth() );
    const int vpHeight = int( anyTargetTexture->getHeight() );
    const Ogre::Matrix4 projMatrix =
        getProjectionMatrix( renderSystem, renderPassDesc->requiresTextureFlipping(), currentCamera, float(vpWidth), float(vpHeight));

    for( size_t i = 0; i < numNeededDraws; ++i )
    {
        auto& cmd = mDrawCmds[i];
        auto *renderable = cmd.renderable;
        assert(renderable);

        auto* pass = renderable->getMaterial()->getTechnique(0u)->getPass(0u);

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
            const bool bShouldBeReadyForPresent = (i + 1) == numNeededDraws;
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

        Ogre::QueuedRenderable queuedRenderable( 0u, renderable, mDummyMovableObject );

        if( cmd.texture )
        {
            pass->getTextureUnitState(0u)->setTexture( cmd.texture );
        }

        //Ogre::Matrix4 finalProjMatrix = projMatrix * cmd.transform;
        //pass->getVertexProgramParameters()->setNamedConstant( "ProjectionMatrix", finalProjMatrix );

        //Ogre::HlmsBlendblock blendblock = *pass->getBlendblock();

        //Ogre::HlmsMacroblock macroblock;
        //macroblock.mCullMode = Ogre::CULL_NONE;
        //macroblock.mScissorTestEnabled = cmd.scissorEnabled;

        //if (cmd.type == CmdType::Geometry)
        //{
        //    blendblock.mBlendChannelMask = Ogre::HlmsBlendblock::BlendChannelAll; 

        //    if (cmd.depthZ > 0.0f)
        //    {
        //        macroblock.mDepthCheck = true;
        //        macroblock.mDepthWrite = false;
        //        macroblock.mDepthFunc = Ogre::CMPF_EQUAL;
        //    }
        //    else
        //    {
        //        macroblock.mDepthCheck = false;
        //        macroblock.mDepthWrite = false;
        //        macroblock.mDepthFunc = Ogre::CMPF_ALWAYS_PASS;
        //    }
        //}
        //else if (cmd.type == CmdType::ClipMask)
        //{
        //    blendblock.mBlendChannelMask = 0; // Невидимая геометрия
    
        //    macroblock.mDepthCheck = true;
        //    macroblock.mDepthWrite = true;
        //    macroblock.mDepthFunc = Ogre::CMPF_ALWAYS_PASS;
        //}

        //pass->setBlendblock(blendblock);
        //pass->setMacroblock(macroblock);

        //Ogre::Matrix4& maskTransform = cmd.transform;
        //maskTransform.setTrans(Ogre::Vector3(cmd.transform.getTrans().x, cmd.transform.getTrans().y, cmd.depthZ));
        //Ogre::Matrix4 finalProjMatrix = projMatrix * maskTransform;
        //pass->getVertexProgramParameters()->setNamedConstant("ProjectionMatrix", finalProjMatrix);
        Ogre::Matrix4 finalProjMatrix = projMatrix * cmd.transform;
        pass->getVertexProgramParameters()->setNamedConstant("ProjectionMatrix", finalProjMatrix);

        const auto *hlmsCache =
            hlms->getMaterial( &c_dummyCache, passCache, queuedRenderable, false, nullptr );

        auto *psoCmd = mCommandBuffer->addCommand<Ogre::CbPipelineStateObject>();
        *psoCmd = Ogre::CbPipelineStateObject( &hlmsCache->pso );

        hlms->fillBuffersForV2( hlmsCache, queuedRenderable, false, 0u, mCommandBuffer );

        Ogre::VertexArrayObject *vao = renderable->getVaos( Ogre::VpNormal ).back();

        OGRE_ASSERT_MEDIUM( vao->getVaoName() != 0u &&
                            "Invalid Vao name! This can happen if a BT_IMMUTABLE buffer was "
                            "recently created and VaoManager::_beginFrame() wasn't called" );

        *mCommandBuffer->addCommand<Ogre::CbVao>() = Ogre::CbVao( vao );
        *mCommandBuffer->addCommand<Ogre::CbIndirectBuffer>() = Ogre::CbIndirectBuffer( mIndirectBuffer );

        void *offset = reinterpret_cast<void *>( mIndirectBuffer->_getFinalBufferStart() +
                                                    sizeof( Ogre::CbDrawIndexed ) * i );

        Ogre::CbDrawCallIndexed *drawCall = mCommandBuffer->addCommand<Ogre::CbDrawCallIndexed>();
        *drawCall = Ogre::CbDrawCallIndexed( baseInstanceAndIndirectBuffers, vao, offset );
        drawCall->numDraws = 1u;

        stats.mDrawCount += 1u;
        stats.mInstanceCount += 1u;
        stats.mFaceCount += vao->getPrimitiveCount() / 3u;
        stats.mVertexCount += vao->getPrimitiveCount();

        hlms->preCommandBufferExecution( mCommandBuffer );
        mCommandBuffer->execute();
        hlms->postCommandBufferExecution( mCommandBuffer );
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

void Manager::SetSceneManager(Ogre::SceneManager& sceneManager)
{
    if (m_sceneManager == &sceneManager)
        return;

    if (mDummyMovableObject && m_sceneManager)
    {
        m_sceneManager->getRootSceneNode(Ogre::SCENE_STATIC)->detachObject(mDummyMovableObject);
        delete mDummyMovableObject;
    }

    m_sceneManager = &sceneManager;
    auto* vaoManager = sceneManager.getDestinationRenderSystem()->getVaoManager();
    mDummyMovableObject = OGRE_NEW ImguiDummyMO(
        Ogre::Id::generateNewId<Ogre::MovableObject>(),
        &sceneManager._getEntityMemoryManager(Ogre::SCENE_STATIC), &sceneManager, 254u);
    sceneManager.getRootSceneNode(Ogre::SCENE_STATIC)->attachObject( mDummyMovableObject );
    mDummyMovableObject->setVisible(false );
    mDummyMovableObject->setCastShadows(false);

}

void Manager::BeginFrame()
{
    mDrawCmds.clear();
    mCurrentScissor = { 0.0f, 0.0f, 1.0f, 1.0f };
    mScissorEnabled = false;
    mClipMaskEnabled = false;
    mCurrentDepthZ = 0.0f;
    mCurrentTransform = Ogre::Matrix4::IDENTITY;
}

 void Manager::EndFrame()
 {
 }

Rml::CompiledGeometryHandle Manager::CompileGeometry(
    Rml::Span<const Rml::Vertex> vertices,
    Rml::Span<const int> indices)
{
    Ogre::VaoManager* vaoManager = Ogre::Root::getSingleton().getRenderSystem()->getVaoManager();
    auto* renderable = new Renderable();
    renderable->updateVertexData(vertices, indices, vaoManager);

    return reinterpret_cast<Rml::CompiledGeometryHandle>(renderable);
}

void Manager::ReleaseGeometry(Rml::CompiledGeometryHandle geometry)
{
    auto* renderable = reinterpret_cast<Renderable*>(geometry);

    auto entry = std::ranges::find(mDrawCmds, renderable, &Command::renderable);
    if (entry != mDrawCmds.end())
        mDrawCmds.erase(entry);

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
    cmd.depthZ = mClipMaskEnabled ? mCurrentDepthZ : 0.0f;
    cmd.type = CmdType::Geometry;
    
    cmd.transform = mCurrentTransform;
    cmd.transform.setTrans(Ogre::Vector3(translation.x, translation.y, 0));

    if (!cmd.texture)
    {
        if (!BlankMaterial)
            createBlankMaterial(m_macroblock, m_blendblock);

		cmd.renderable->setMaterial(BlankMaterial);

        //if (!BlankDatablock)
        //    createBlankDatablock(m_macroblock, m_blendblock);

        //cmd.renderable->setDatablock(BlankDatablock);
    }
    else if (!cmd.renderable->getMaterial())
        cmd.renderable->setMaterial(createMaterialFor(cmd.texture));
}

void Manager::EnableScissorRegion(bool enable)
{
    mScissorEnabled = enable;
}

void Manager::SetScissorRegion(Rml::Rectanglei region)
{
    mCurrentScissor = Ogre::Vector4(region.Left(), region.Top(), region.Right(), region.Bottom());
}

void Manager::SetTransform(const Rml::Matrix4f* transform) {
    if (transform)
        mCurrentTransform = Ogre::Matrix4(transform->data()).transpose();
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
    std::size_t size = Ogre::PixelFormatGpuUtils::calculateSizeBytes(
        source_dimensions.x, source_dimensions.y, 1u, 1u, Ogre::PixelFormatGpu::PFG_RGBA8_UNORM_SRGB, 1u, 4u);
    Ogre::uint8* data = reinterpret_cast<Ogre::uint8*>(
        OGRE_MALLOC_SIMD(size, Ogre::MEMCATEGORY_GENERAL));
    std::copy(source.begin(), source.end(), data);

    auto* image = OGRE_NEW Ogre::Image2;
    image->loadDynamicImage(
        data,
        source_dimensions.x,
        source_dimensions.y,
        1u,
        Ogre::TextureTypes::Type2D,
        Ogre::PixelFormatGpu::PFG_RGBA8_UNORM,
        true,
        1u);

    Ogre::TextureGpu* texture = textureManager->createTexture(
        texName,
        Ogre::GpuPageOutStrategy::AlwaysKeepSystemRamCopy,
        0,
        Ogre::TextureTypes::Type2D,
        Ogre::BLANKSTRING);
    texture->setNumMipmaps(1);
    texture->setResolution(source_dimensions.x, source_dimensions.y);
    texture->setPixelFormat(Ogre::PixelFormatGpu::PFG_RGBA8_UNORM);

    texture->scheduleTransitionTo(Ogre::GpuResidency::Resident, image);

    //Ogre::TextureGpu* tex = textureManager->createTexture(
    //    texName, Ogre::GpuPageOutStrategy::AlwaysKeepSystemRamCopy,
    //    Ogre::TextureFlags::ManualTexture, Ogre::TextureTypes::Type2D);
    //    
    //tex->setResolution(source_dimensions.x, source_dimensions.y);
    //tex->setPixelFormat(Ogre::PixelFormatGpu::PFG_RGBA8_UNORM_SRGB);
    //
    //Ogre::Image2 image;
    //image.createEmptyImageLike(tex);
    //Ogre::TextureBox dstBox = image.getData(0u);
    //
    //const Rml::byte* srcData = source.data();
    //for(uint32_t y = 0u; y < dstBox.height; ++y) {
    //    void *dstRaw = dstBox.at(0u, y, 0u);
    //    memcpy(dstRaw, &srcData[y * dstBox.width * 4u], dstBox.width * 4u);
    //}
    //
    //tex->scheduleTransitionTo(Ogre::GpuResidency::Resident);
    //image.uploadTo(tex, 0u, tex->getNumMipmaps() - 1u);

    // Tweak via _setAutoDelete so the internal data is copied as a pointer
    // instead of performing a deep copy of the data; while leaving the responsability
    // of freeing memory to imagePtr instead.
    //image._setAutoDelete( false );
    //auto imagePtr = new Ogre::Image2( image );
    //imagePtr->_setAutoDelete( true );

    //if (tex->getNextResidencyStatus() == Ogre::GpuResidency::Resident)
    //    tex->scheduleTransitionTo(Ogre::GpuResidency::OnStorage);
    //// Ogre will call "delete imagePtr" when done, because we're passing
    //// true to autoDeleteImage argument in scheduleTransitionTo.
    //tex->scheduleTransitionTo(Ogre::GpuResidency::Resident, imagePtr, true);
    
    return reinterpret_cast<Rml::TextureHandle>(texture);
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

void Manager::EnableClipMask(bool enable)
{
    mClipMaskEnabled = enable;
    //if (!enable)
    //    mCurrentDepthZ = 0.0f;
}

void Manager::RenderToClipMask(
	Rml::ClipMaskOperation operation,
	Rml::CompiledGeometryHandle geometry,
	Rml::Vector2f translation)
{
    mCurrentDepthZ += 0.02f;

    auto& cmd = mDrawCmds.emplace_back();
    cmd.renderable = reinterpret_cast<Renderable*>(geometry);
    cmd.texture = nullptr;
    cmd.scissor = mCurrentScissor;
    cmd.scissorEnabled = mScissorEnabled;
    cmd.depthZ = mCurrentDepthZ;
    cmd.type = CmdType::ClipMask;
    
    cmd.transform = mCurrentTransform;
    cmd.transform.setTrans(Ogre::Vector3(translation.x, translation.y, 0));

    if (!BlankMaterial)
        createBlankMaterial(m_macroblock, m_blendblock);

	cmd.renderable->setMaterial(BlankMaterial);
}

Rml::LayerHandle Manager::PushLayer()
{
    int a = 1;
    return {};

	//Layer oldTopLayer{this->addConnection(), -1};
	//this->putLayerBuffer(-1, oldTopLayer);
	//Layer newLayer = this->acquireLayerBuffer();
	//this->passes.push_back(SwapPass(newLayer.connectionId, oldTopLayer.connectionId));
	//this->passes.push_back(StartLayerPass{});

	//return Rml::LayerHandle(this->numActiveLayers - 1);
}
void Manager::CompositeLayers(
	Rml::LayerHandle source,
	Rml::LayerHandle destination,
	Rml::BlendMode blend_mode,
	Rml::Span<const Rml::CompiledFilterHandle> filters)
{
    int a = 1;

	//Layer topLayer{this->addConnection(), -1};
	//Layer sourceLayer;
	//Layer destinationLayer;

	//bool sourceIsTopLayer = static_cast<int>(source) == this->numActiveLayers - 1;
	//bool destinationIsTopLayer = static_cast<int>(destination) == this->numActiveLayers - 1;

	//Layer tempLayer = this->acquireLayerBuffer();
	//if(sourceIsTopLayer)
	//{
	//	topLayer.copyPass = this->passes.size();
	//	this->passes.push_back(CopyPass(tempLayer.connectionId, topLayer.connectionId));
	//}
	//else
	//{
	//	this->passes.push_back(SwapPass(this->getLayerBuffer(source).connectionId, topLayer.connectionId));
	//	sourceLayer = Layer{this->addConnection(), static_cast<int>(this->passes.size())};
	//	this->passes.push_back(CopyPass(tempLayer.connectionId, sourceLayer.connectionId));
	//}

	//if(destinationIsTopLayer)
	//{
	//	destinationLayer = topLayer;
	//	topLayer = Layer{};
	//}
	//else if(source == destination)
	//{
	//	destinationLayer = sourceLayer;
	//	sourceLayer = Layer{};
	//}
	//else
	//	destinationLayer = this->layerBuffers.at(destination);

	//// Change source layer to copy (if destination != source)
	//if(!sourceLayer.isTaken())
	//	this->putLayerBuffer(source, sourceLayer);


	//for(auto filter : filters)
	//	this->filters.at(filter)->apply(*this);


	//tempLayer = Layer{this->addConnection(), -1};
	//if(this->renderPassSettings.enableStencil)
	//{
	//	this->passes.push_back(CompositeWithStencilPass(
	//		destinationLayer.connectionId,
	//		tempLayer.connectionId,
	//		blend_mode == Rml::BlendMode::Replace,
	//		this->renderPassSettings));
	//}
	//else
	//{
	//	this->passes.push_back(CompositePass(
	//		destinationLayer.connectionId,
	//		tempLayer.connectionId,
	//		blend_mode == Rml::BlendMode::Replace,
	//		this->renderPassSettings));
	//}
	//this->releaseLayerBuffer(tempLayer);

	//if(!destinationIsTopLayer)
	//{
	//	destinationLayer = Layer{this->addConnection(), static_cast<int>(this->passes.size())};
	//	this->passes.push_back(SwapPass(topLayer.connectionId, destinationLayer.connectionId));
	//	this->putLayerBuffer(destination, destinationLayer);
	//	this->putLayerBuffer(-1, Layer{-1, topLayer.copyPass});
	//}
}

void Manager::PopLayer()
{
    int a = 1;
	//Layer poppedLayer = this->getLayerBuffer(-1);
	//Layer newTopLayer = this->getLayerBuffer(-2);
	//auto* lastPass = std::get_if<SwapPass>(&this->passes.back());
	//if(lastPass)
	//{
	//	CopyPass* copyPass = nullptr;
	//	if(poppedLayer.copyPass != -1)
	//		copyPass = std::get_if<CopyPass>(&this->passes[poppedLayer.copyPass]);
	//	if(copyPass && lastPass->swapIn == copyPass->copyOut)
	//	{
	//		this->releaseLayerBuffer(Layer{copyPass->copyIn, -1});
	//		this->passes[poppedLayer.copyPass] = NullPass{};
	//		if(newTopLayer.connectionId == lastPass->swapOut)
	//			this->passes.pop_back();
	//		else
	//			this->passes.back() = SwapPass(newTopLayer.connectionId, lastPass->swapOut);
	//	}
	//	else
	//	{
	//		this->releaseLayerBuffer(Layer{lastPass->swapIn, -1});
	//		if(newTopLayer.connectionId == lastPass->swapOut)
	//			this->passes.pop_back();
	//		else
	//			this->passes.back() = SwapPass(newTopLayer.connectionId, lastPass->swapOut);
	//	}
	//}
	//else
	//{
	//	Layer poppedLayer{this->addConnection(), -1};
	//	this->releaseLayerBuffer(poppedLayer);
	//	this->passes.push_back(SwapPass{newTopLayer.connectionId, poppedLayer.connectionId});
	//}
}

Rml::CompiledFilterHandle Manager::CompileFilter(
	const Rml::String& name,
	const Rml::Dictionary& parameters)
{
    return {};
	//auto maker = this->filterMakers.find(name);
	//if(maker == this->filterMakers.end())
	//		return {};

	//auto filter = maker->second->make(parameters);
	//auto handle = this->filters.insert(std::move(filter));
	//return handle;
}

void Manager::ReleaseFilter(Rml::CompiledFilterHandle filter)
{
	int a = 1;
}

Rml::TextureHandle Manager::SaveLayerAsTexture()
{
    return {};
}

Rml::CompiledFilterHandle Manager::SaveLayerAsMaskImage()
{
    return {};
}

Rml::CompiledShaderHandle Manager::CompileShader(
	const Rml::String& name,
	const Rml::Dictionary& parameters)
{
    return {};
}

void Manager::RenderShader(
	Rml::CompiledShaderHandle shader,
	Rml::CompiledGeometryHandle geometry,
	Rml::Vector2f translation,
	Rml::TextureHandle texture)
{
	int a = 1;
}

void Manager::ReleaseShader(Rml::CompiledShaderHandle shader)
{
	int a = 1;
}
