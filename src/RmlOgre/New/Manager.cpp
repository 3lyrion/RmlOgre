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
#include <OgreIdString.h>

#include <numeric>

using namespace RmlOgre;

namespace
{
    Ogre::VaoManager*        VAOManager     = nullptr;
    Ogre::TextureGpuManager* TextureManager = nullptr;

    const Ogre::HlmsCache c_dummyCache( 0, Ogre::HLMS_MAX, Ogre::HLMS_CACHE_FLAGS_NONE, Ogre::HlmsPso() );

    class RmlUiDummyMO final : public Ogre::MovableObject
    {
    public:
        RmlUiDummyMO( Ogre::IdType id, Ogre::ObjectMemoryManager *objectMemoryManager, Ogre::SceneManager *manager,
                      Ogre::uint8 renderQueueId ) :
            MovableObject( id, objectMemoryManager, manager, renderQueueId )
        {
        }

        // Overrides from MovableObject
        const Ogre::String &getMovableType() const final { return Ogre::BLANKSTRING; }
    };
}

//-----------------------------------------------------------------------------
Manager::Manager()
{
    m_commandBuffer = new Ogre::CommandBuffer();
    createPrograms();

    m_samplerblock.setFiltering(Ogre::TFO_NONE);
    m_samplerblock.setAddressingMode(Ogre::TAM_CLAMP);

    m_blendblock.mBlendOperation = Ogre::SceneBlendOperation::SBO_ADD;
    m_blendblock.mDestBlendFactor = Ogre::SceneBlendFactor::SBF_ONE_MINUS_SOURCE_ALPHA;
    m_blendblock.mSourceBlendFactor = Ogre::SceneBlendFactor::SBF_ONE;

    m_macroblock.mScissorTestEnabled = true;
    m_macroblock.mDepthCheck = false;
    m_macroblock.mDepthWrite = false;
    m_macroblock.mCullMode = Ogre::CULL_NONE;

    m_drawCmds.reserve(m_poolAllocSize);
    m_renderablePool.resize(m_poolAllocSize);
    m_transformPool.resize(m_poolAllocSize);
}
//-----------------------------------------------------------------------------
Manager::~Manager()
{
    destroyAllResources();
    delete m_dummyMovableObject;
    m_dummyMovableObject = 0;
    delete m_commandBuffer;
    m_commandBuffer = 0;
}
//-----------------------------------------------------------------------------
void Manager::destroyAllResources()
{
    m_shaderMakers.clear();

    if( m_indirectBuffer )
    {
        if (m_indirectBuffer->getMappingState() != Ogre::MS_UNMAPPED)
            m_indirectBuffer->unmap(Ogre::UO_UNMAP_ALL);
        VAOManager->destroyIndirectBuffer( m_indirectBuffer );
        m_indirectBuffer = 0;
    }

    m_commandBuffer->clear();
}

void Manager::OnResourcesLoaded()
{
    auto* renderSystem = Ogre::Root::getSingleton().getRenderSystem();
    VAOManager     = renderSystem->getVaoManager();
    TextureManager = renderSystem->getTextureGpuManager();

    createBlankTexture();
    createBlankMaterial();
    createBaseMaterial();
    createMaskMaterial();
}

void Manager::createBaseMaterial()
{
    const Ogre::String materialName = "!!OgreRmlUi_BaseMat";
    
    m_baseMaterial = Ogre::MaterialManager::getSingleton().create(
        materialName, Ogre::ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME );

    Ogre::Pass *pass = m_baseMaterial->getTechnique( 0 )->getPass( 0 );
    pass->setFragmentProgram( "imgui/FP" );
    pass->setVertexProgram( "imgui/VP" );

    pass->setSamplerblock(m_samplerblock );
    pass->setBlendblock(m_blendblock);
    pass->setMacroblock(m_macroblock);

    auto textureUnit = pass->createTextureUnitState();
}

void Manager::createBlankTexture()
{
    auto* textureManager = Ogre::Root::getSingleton().getRenderSystem()->getTextureGpuManager();

    m_blankTexture = textureManager->createTexture("RmlUi/BlankTexture",
        Ogre::GpuPageOutStrategy::AlwaysKeepSystemRamCopy,
        0,
        Ogre::TextureTypes::Type2D,
        Ogre::BLANKSTRING);
    m_blankTexture->setNumMipmaps(1);
    m_blankTexture->setResolution( 1u, 1u );
    m_blankTexture->setPixelFormat( Ogre::PixelFormatGpu::PFG_RGBA8_UNORM );

    auto* image = OGRE_NEW Ogre::Image2;
    image->createEmptyImageLike( m_blankTexture );
    Ogre::TextureBox dstBox = image->getData( 0u );
    
    uint32_t whitePixel = 0xFFFFFFFF;
    memcpy( dstBox.data, &whitePixel, sizeof( uint32_t ) );

    m_blankTexture->scheduleTransitionTo(Ogre::GpuResidency::Resident, image);
}

void Manager::createBlankMaterial()
{
    const Ogre::String materialName = "!!OgreRmlUi_BlankMat";
    
    m_blankMaterial = Ogre::MaterialManager::getSingleton().create(
        materialName, Ogre::ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME );

    Ogre::Pass *pass = m_blankMaterial->getTechnique( 0 )->getPass( 0 );
    pass->setFragmentProgram( "imgui/FP" );
    pass->setVertexProgram( "imgui/VP" );

    pass->setSamplerblock( m_samplerblock );
    pass->setBlendblock( m_blendblock );
    pass->setMacroblock( m_macroblock );

    auto textureUnit = pass->createTextureUnitState();
    textureUnit->setTexture( m_blankTexture );
}

void Manager::createMaskMaterial()
{
    const Ogre::String materialName = "!!OgreRmlUi_MaskMat";

    m_maskMaterial = Ogre::MaterialManager::getSingleton().create(
        materialName, Ogre::ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME );

    Ogre::Pass *pass = m_maskMaterial->getTechnique( 0 )->getPass( 0 );
    pass->setFragmentProgram( "imgui/FP" );
    pass->setVertexProgram( "imgui/VP" );

    Ogre::HlmsBlendblock maskBlendblock = m_blendblock;
    maskBlendblock.mBlendChannelMask = 0;

    pass->setBlendblock(maskBlendblock);
    pass->setMacroblock(m_macroblock);
}


//-----------------------------------------------------------------------------
Ogre::Matrix4 Manager::getProjectionMatrix( Ogre::RenderSystem* rs, const bool bRequiresTextureFlipping,
                                           const Ogre::Camera* currentCamera, float vpWidth, float vpHeight ) const
{
    Ogre::Matrix4 projectionMatrix{ 2.0f / vpWidth, 0.0f            , 0.0f , -1.0f,
                                    0.0f          , -2.0f / vpHeight, 0.0f , 1.0f ,
                                    0.0f          , 0.0f            , -1.0f, 0.0f ,
                                    0.0f          , 0.0f            , 0.0f , 1.0f  };
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
void Manager::drawIntoCompositor( Ogre::RenderPassDescriptor* renderPassDesc,
                                       Ogre::TextureGpu* anyTargetTexture, Ogre::SceneManager *sceneManager,
                                       const Ogre::Camera* currentCamera )
{

    Ogre::RenderSystem *renderSystem = sceneManager->getDestinationRenderSystem();
    const bool supportsIndirectBuffers = VAOManager->supportsIndirectBuffers();
    const size_t numNeededDraws = m_drawCmds.size();

    const bool bWasReadyForPresent = renderPassDesc->mReadyWindowForPresent;
    const Ogre::Vector4 viewportSize( 0, 0, 1, 1 );
    Ogre::RenderingMetrics stats;

    unsigned char *indirectDraw = 0;
    if (numNeededDraws > 0)
    {
        if (!m_indirectBuffer || (numNeededDraws * sizeof(Ogre::CbDrawIndexed)) > m_indirectBuffer->getNumElements())
        {
            if (m_indirectBuffer)
            {
                if (m_indirectBuffer->getMappingState() != Ogre::MS_UNMAPPED)
                    m_indirectBuffer->unmap( Ogre::UO_UNMAP_ALL );
                VAOManager->destroyIndirectBuffer( m_indirectBuffer );
            }
            m_indirectBuffer = VAOManager->createIndirectBuffer( numNeededDraws * sizeof(Ogre::CbDrawIndexed),
                                                                Ogre::BT_DYNAMIC_PERSISTENT, 0, false);
        }

        if( supportsIndirectBuffers )
            indirectDraw = static_cast<unsigned char*>(m_indirectBuffer->map(0, m_indirectBuffer->getNumElements()));
        else
            indirectDraw = m_indirectBuffer->getSwBufferPtr();

        for (size_t i = 0; i < numNeededDraws; ++i)
        {
            auto& cmd = m_drawCmds[i];
            if (cmd.renderableIndex == UINT16_MAX)
                continue;

            auto& renderable = m_renderablePool[cmd.renderableIndex];
            auto* vao        = renderable.getVaos(Ogre::VpNormal).back();

            auto& cbCmd = *reinterpret_cast<Ogre::CbDrawIndexed*>(indirectDraw);
            indirectDraw += sizeof(Ogre::CbDrawIndexed);
            
            cbCmd.primCount        = vao->getPrimitiveCount();
            cbCmd.instanceCount    = 1;
            cbCmd.firstVertexIndex = uint32_t( vao->getIndexBuffer()->_getFinalBufferStart() + vao->getPrimitiveStart() );
            cbCmd.baseVertex       = uint32_t( vao->getBaseVertexBuffer()->_getFinalBufferStart() );
            cbCmd.baseInstance     = 0;
        }

        if( indirectDraw && supportsIndirectBuffers )
            m_indirectBuffer->unmap( Ogre::UO_KEEP_PERSISTENT );
    }

    Ogre::HlmsManager *hlmsManager = Ogre::Root::getSingleton().getHlmsManager();
    Ogre::Hlms *hlms = hlmsManager->getHlms( Ogre::HLMS_LOW_LEVEL );

    m_commandBuffer->setCurrentRenderSystem( renderSystem );

    int baseInstanceAndIndirectBuffers = 0;
    if( VAOManager->supportsIndirectBuffers() )
        baseInstanceAndIndirectBuffers = 2;
    else if( VAOManager->supportsBaseInstance() )
        baseInstanceAndIndirectBuffers = 1;

    Ogre::HlmsCache passCache = hlms->preparePassHash( 0, false, false, sceneManager );

    const int vpWidth = int( anyTargetTexture->getWidth() );
    const int vpHeight = int( anyTargetTexture->getHeight() );
    const Ogre::Matrix4 projMatrix =
        getProjectionMatrix( renderSystem, renderPassDesc->requiresTextureFlipping(), currentCamera, float(vpWidth), float(vpHeight));

    auto translationMatrix = Ogre::Matrix4::IDENTITY;

    for( size_t i = 0; i < numNeededDraws; ++i )
    {
        auto& cmd = m_drawCmds[i];
        if (cmd.renderableIndex == UINT16_MAX)
            continue;

        auto& renderable = m_renderablePool[cmd.renderableIndex];
        auto* vao        = renderable.getVaos(Ogre::VpNormal).back();
        OGRE_ASSERT_MEDIUM( vao->getVaoName() != 0u &&
                    "Invalid Vao name! This can happen if a BT_IMMUTABLE buffer was "
                    "recently created and VaoManager::_beginFrame() wasn't called" );

        auto* pass = renderable.getMaterial()->getTechnique(0u)->getPass(0u);

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

        Ogre::StencilParams stencilParams;
        if (cmd.type == CmdType::ClipMask)
        {
            stencilParams.enabled = true;

            if (cmd.clipMaskOp == ClipMaskOperation::Set || cmd.clipMaskOp == ClipMaskOperation::SetInverse)
            {
                stencilParams.stencilFront.compareOp = Ogre::CMPF_ALWAYS_PASS;
                stencilParams.stencilFront.stencilPassOp = Ogre::SOP_REPLACE;
                stencilParams.stencilBack = stencilParams.stencilFront;
                renderSystem->setStencilBufferParams(cmd.stencilValue, stencilParams);
            }
            else if (cmd.clipMaskOp == ClipMaskOperation::Intersect)
            {
                stencilParams.stencilFront.compareOp = Ogre::CMPF_EQUAL;
                stencilParams.stencilFront.stencilPassOp = Ogre::SOP_INCREMENT;
                stencilParams.stencilBack = stencilParams.stencilFront;
                // Previous
                renderSystem->setStencilBufferParams(cmd.stencilValue - 1, stencilParams);
            }
        }
        else if (cmd.type == CmdType::Geometry)
        {
            if (cmd.clipMaskOp != ClipMaskOperation::None)
            {
                stencilParams.enabled = true;
                stencilParams.stencilFront.stencilPassOp = Ogre::SOP_KEEP;
                stencilParams.stencilFront.compareOp = cmd.clipMaskOp == ClipMaskOperation::SetInverse ? Ogre::CMPF_NOT_EQUAL : Ogre::CMPF_EQUAL;
                stencilParams.stencilBack = stencilParams.stencilFront;
                renderSystem->setStencilBufferParams(cmd.stencilValue, stencilParams);
            }
            else
            {
                stencilParams.enabled = false;
                renderSystem->setStencilBufferParams(0, stencilParams);
            }
        }
        
        renderSystem->beginRenderPassDescriptor( renderPassDesc, anyTargetTexture, 0u, &viewportSize,
                                                 &scissors, 1u, false, false );
        renderSystem->executeRenderPassDescriptorDelayedActions();

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

        if (cmd.texture)
        {
            auto* textureUnit = pass->getTextureUnitState(0);
            textureUnit->setTexture(cmd.texture);
        }

        Ogre::QueuedRenderable queuedRenderable(0, &renderable, m_dummyMovableObject);

        translationMatrix.setTrans(Ogre::Vector3(cmd.translation.x, cmd.translation.y, 0));

        auto finalProjMatrix = (cmd.transformIndex == UINT16_MAX)
            ? projMatrix * translationMatrix
            : projMatrix * m_transformPool[cmd.transformIndex] * translationMatrix;
        pass->getVertexProgramParameters()->setNamedConstant("ProjectionMatrix", finalProjMatrix);

        const auto *hlmsCache =
            hlms->getMaterial( &c_dummyCache, passCache, queuedRenderable, false, nullptr );

        auto *psoCmd = m_commandBuffer->addCommand<Ogre::CbPipelineStateObject>();
        *psoCmd = Ogre::CbPipelineStateObject( &hlmsCache->pso );

        hlms->fillBuffersForV2( hlmsCache, queuedRenderable, false, 0u, m_commandBuffer );

        *m_commandBuffer->addCommand<Ogre::CbVao>() = Ogre::CbVao( vao );
        *m_commandBuffer->addCommand<Ogre::CbIndirectBuffer>() = Ogre::CbIndirectBuffer( m_indirectBuffer );

        void *offset = reinterpret_cast<void *>( m_indirectBuffer->_getFinalBufferStart() +
                                                    sizeof( Ogre::CbDrawIndexed ) * i );

        Ogre::CbDrawCallIndexed *drawCall = m_commandBuffer->addCommand<Ogre::CbDrawCallIndexed>();
        *drawCall = Ogre::CbDrawCallIndexed( baseInstanceAndIndirectBuffers, vao, offset );
        drawCall->numDraws = 1u;

        stats.mDrawCount += 1u;
        stats.mInstanceCount += 1u;
        stats.mFaceCount += vao->getPrimitiveCount() / 3u;
        stats.mVertexCount += vao->getPrimitiveCount();

        hlms->preCommandBufferExecution( m_commandBuffer );
        m_commandBuffer->execute();
        hlms->postCommandBufferExecution( m_commandBuffer );
    }

    renderSystem->_addMetrics( stats );

    // There was nothing for RmlUi to draw. We must still prepare the window for presenting.
    if (bWasReadyForPresent && !stats.mDrawCount)
    {
        Ogre::Vector4 scissors( 0, 0, 1, 1 );
        renderSystem->beginRenderPassDescriptor( renderPassDesc, anyTargetTexture, 0u, &viewportSize,
                                                 &scissors, 1u, false, false );
        renderSystem->executeRenderPassDescriptorDelayedActions();
    }
}

void Manager::AddShaderMaker(std::string_view name, std::unique_ptr<ShaderMaker>&& maker)
{
    m_shaderMakers[StringHasher(name)] = std::move(maker);
}

void Manager::SetSceneManager(Ogre::SceneManager& sceneManager)
{
    if (m_sceneManager == &sceneManager)
        return;

    if (m_dummyMovableObject && m_sceneManager)
    {
        m_sceneManager->getRootSceneNode(Ogre::SCENE_STATIC)->detachObject(m_dummyMovableObject);
        delete m_dummyMovableObject;
    }

    m_sceneManager = &sceneManager;
    m_dummyMovableObject = OGRE_NEW RmlUiDummyMO(
        Ogre::Id::generateNewId<Ogre::MovableObject>(),
        &sceneManager._getEntityMemoryManager(Ogre::SCENE_STATIC), &sceneManager, 254u);
    sceneManager.getRootSceneNode(Ogre::SCENE_STATIC)->attachObject( m_dummyMovableObject );
    m_dummyMovableObject->setVisible(false );
    m_dummyMovableObject->setCastShadows(false);

}

void Manager::BeginFrame()
{
    m_drawCmds.clear();
    //m_garbageDrawCmds.clear();
    m_scissorRef = { 0.0f, 0.0f, 1.0f, 1.0f };
    m_scissorEnabled = false;
    m_clipMaskEnabled = false;
    m_stencilRefValue = 0;
    m_stencilBaseValue = 0;
    m_transformRefIndex = UINT16_MAX;
    m_freeRenderables.resize(m_poolAllocSize);
    std::iota(m_freeRenderables.rbegin(), m_freeRenderables.rend(), 0);
    m_freeTransforms.resize(m_poolAllocSize);
    std::iota(m_freeTransforms.rbegin(), m_freeTransforms.rend(), 0);
}

 void Manager::EndFrame()
 {
     // Nothing
 }

Rml::CompiledGeometryHandle Manager::CompileGeometry(
    Rml::Span<const Rml::Vertex> vertices,
    Rml::Span<const int> indices)
{
    auto index = m_freeRenderables.back();
    m_freeRenderables.pop_back();

    auto& renderable = m_renderablePool[index];
    renderable.updateVertexData(vertices, indices, VAOManager);

    return ++index;
}

void Manager::ReleaseGeometry(Rml::CompiledGeometryHandle geometry)
{
    auto index = static_cast<uint16_t>(geometry) - 1;
    m_freeRenderables.push_back(index);

    auto& renderable = m_renderablePool[index];
    auto& cmd        = m_drawCmds[renderable.getOwningCommandIndex()];
    cmd.renderableIndex = UINT16_MAX;
    renderable.destroyBuffers(VAOManager);
}

void Manager::RenderGeometry(
        Rml::CompiledGeometryHandle geometry,
        Rml::Vector2f translation,
        Rml::TextureHandle texture) 
{
    auto index = static_cast<uint16_t>(geometry) - 1;

    auto& cmd = m_drawCmds.emplace_back();
    cmd.renderableIndex = index;
    cmd.texture         = reinterpret_cast<Ogre::TextureGpu*>(texture);
    cmd.scissor         = m_scissorRef;
    cmd.scissorEnabled  = m_scissorEnabled;
    cmd.type            = CmdType::Geometry;
    if (m_clipMaskEnabled)
    {
        cmd.stencilValue = m_stencilRefValue;
        cmd.clipMaskOp = m_clipMaskOpRef;
    }
    cmd.translation     = translation;
    cmd.transformIndex  = m_transformRefIndex;

    auto& renderable = m_renderablePool[index];
    renderable.setOwningCommandIndex(uint16_t(m_drawCmds.size() - 1));
    renderable.setMaterial(texture ? m_baseMaterial : m_blankMaterial);
}

void Manager::EnableScissorRegion(bool enable)
{
    m_scissorEnabled = enable;
}

void Manager::SetScissorRegion(Rml::Rectanglei region)
{
    m_scissorRef = Ogre::Vector4{ (float)region.Left(), (float)region.Top(), (float)region.Right(), (float)region.Bottom() };
}

void Manager::SetTransform(const Rml::Matrix4f* transform)
{
    if (!transform)
    {
        if (m_transformRefIndex != UINT16_MAX)
        {
            m_freeTransforms.push_back(m_transformRefIndex);
            m_transformRefIndex = UINT16_MAX;
        }
        return;
    }

    auto index = m_freeTransforms.back();
    m_freeTransforms.pop_back();
    m_transformPool[index] = Ogre::Matrix4(transform->data()).transpose();
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

    Ogre::TextureGpu* texture = TextureManager->createTexture(
        texName,
        Ogre::GpuPageOutStrategy::AlwaysKeepSystemRamCopy,
        0,
        Ogre::TextureTypes::Type2D,
        Ogre::BLANKSTRING);
    texture->setNumMipmaps(1);
    texture->setResolution(uint32_t(source_dimensions.x), uint32_t(source_dimensions.y));
    texture->setPixelFormat(Ogre::PixelFormatGpu::PFG_RGBA8_UNORM);
    texture->scheduleTransitionTo(Ogre::GpuResidency::Resident, image);
 
    return reinterpret_cast<Rml::TextureHandle>(texture);
}

void Manager::ReleaseTexture(Rml::TextureHandle texture_handle)
{
    auto* tex = reinterpret_cast<Ogre::TextureGpu*>(texture_handle);
    if (tex)
        TextureManager->destroyTexture(tex);
}

void Manager::EnableClipMask(bool enable)
{
    m_clipMaskEnabled = enable;
}

void Manager::RenderToClipMask(
    Rml::ClipMaskOperation operation,
    Rml::CompiledGeometryHandle geometry,
    Rml::Vector2f translation)
{
    auto index = static_cast<uint16_t>(geometry) - 1;

    if (operation == Rml::ClipMaskOperation::Set || operation == Rml::ClipMaskOperation::SetInverse)
    {
        m_stencilBaseValue += 2;
        m_stencilRefValue = m_stencilBaseValue + 1;
    }
    else if (operation == Rml::ClipMaskOperation::Intersect)
    {
        m_stencilRefValue += 1;
    }
    m_clipMaskOpRef = ClipMaskOperation(operation);

    auto& cmd = m_drawCmds.emplace_back();
    cmd.renderableIndex = index;
    cmd.texture         = nullptr;
    cmd.scissor         = m_scissorRef;
    cmd.scissorEnabled  = m_scissorEnabled;
    cmd.type            = CmdType::ClipMask;
    cmd.stencilValue    = m_stencilRefValue;
    cmd.clipMaskOp      = m_clipMaskOpRef;
    cmd.translation     = translation;
    cmd.transformIndex  = m_transformRefIndex;

    auto& renderable = m_renderablePool[index];
    renderable.setOwningCommandIndex(uint16_t(m_drawCmds.size() - 1));
    renderable.setMaterial(m_maskMaterial);
}

Rml::LayerHandle Manager::PushLayer()
{
    return {};
}

void Manager::CompositeLayers(
    Rml::LayerHandle source,
    Rml::LayerHandle destination,
    Rml::BlendMode blend_mode,
    Rml::Span<const Rml::CompiledFilterHandle> filters)
{

}

void Manager::PopLayer()
{

}

Rml::CompiledFilterHandle Manager::CompileFilter(
    const Rml::String& name,
    const Rml::Dictionary& parameters)
{
    return {};
}

void Manager::ReleaseFilter(Rml::CompiledFilterHandle filter)
{

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
    auto hash = StringHasher(name);

    auto entry = m_shaderMakers.find(hash);
    if (entry == m_shaderMakers.end())
        throw std::runtime_error("RmlOgre: ShaderMaker with name '" + name + "' not found");

    auto& shaderMaker = entry->second;
    m_shaderMaterials.emplace(++m_shaderIdCounter, shaderMaker->make(parameters));
    return m_shaderIdCounter;
}

void Manager::RenderShader(
    Rml::CompiledShaderHandle shader,
    Rml::CompiledGeometryHandle geometry,
    Rml::Vector2f translation,
    Rml::TextureHandle texture)
{
    if (texture)
        throw std::runtime_error("RmlOgre: RenderShader does not support texture. Set the texture from your ShaderMaker");

    if (!shader)
        throw std::runtime_error("RmlOgre: RenderShader called with null shader");

    auto index = static_cast<uint16_t>(geometry) - 1;

    auto& cmd = m_drawCmds.emplace_back();
    cmd.id              = ++m_cmdIdCounter;
    cmd.renderableIndex = index;
    cmd.texture         = nullptr;
    cmd.scissor         = m_scissorRef;
    cmd.scissorEnabled  = m_scissorEnabled;
    cmd.type            = CmdType::Geometry;
    if (m_clipMaskEnabled)
    {
        cmd.stencilValue = m_stencilRefValue;
        cmd.clipMaskOp   = m_clipMaskOpRef;
    }
    cmd.translation     = translation;
    cmd.transformIndex  = m_transformRefIndex;

    auto& renderable = m_renderablePool[index];
    auto& mat = m_shaderMaterials.at(shader);
    renderable.setOwningCommandIndex(uint16_t(m_drawCmds.size() - 1));
    renderable.setMaterial(mat);
}

void Manager::ReleaseShader(Rml::CompiledShaderHandle shader)
{
    m_shaderMaterials.erase(shader);
}
