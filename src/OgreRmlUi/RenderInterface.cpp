#include <OgreRmlUi/RenderInterface.h>

#include <OgreRmlUi/Renderable.h>
#include <OgreRmlUi/Filters.h>
#include <OgreRmlUi/Shaders.h>

#include "CommandBuffer/OgreCbDrawCall.h"
#include "CommandBuffer/OgreCbPipelineStateObject.h"
#include "CommandBuffer/OgreCbShaderBuffer.h"
#include "CommandBuffer/OgreCommandBuffer.h"
#include "OgreCamera.h"
#include "OgreHighLevelGpuProgramManager.h"
#include "OgreHlms.h"
#include "OgreHlmsUnlit.h"
#include "OgreHlmsUnlitDatablock.h"
#include <OgreHlmsPbs.h>
#include "OgreHlmsManager.h"
#include "OgreHlmsUnlitDatablock.h"
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

using namespace OgreRmlUi;

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

RenderInterface::RenderInterface()
{
    m_commandBuffer = OGRE_NEW Ogre::CommandBuffer();

    m_samplerblock.setFiltering(Ogre::TFO_NONE);
    m_samplerblock.setAddressingMode(Ogre::TAM_CLAMP);

    m_blendblock.mBlendOperation = Ogre::SceneBlendOperation::SBO_ADD;
    m_blendblock.mDestBlendFactor = Ogre::SceneBlendFactor::SBF_ONE_MINUS_SOURCE_ALPHA;
    m_blendblock.mSourceBlendFactor = Ogre::SceneBlendFactor::SBF_ONE;

    m_macroblock.mScissorTestEnabled = true;
    m_macroblock.mDepthCheck = false;
    m_macroblock.mDepthWrite = false;
    m_macroblock.mCullMode = Ogre::CULL_NONE;
}

RenderInterface::~RenderInterface()
{
    if( m_indirectBuffer )
    {
        if (m_indirectBuffer->getMappingState() != Ogre::MS_UNMAPPED)
            m_indirectBuffer->unmap(Ogre::UO_UNMAP_ALL);
        VAOManager->destroyIndirectBuffer( m_indirectBuffer );
        m_indirectBuffer = 0;
    }

    m_commandBuffer->clear();

    for (auto& cmd : m_drawCommands)
    {
        if (!cmd.renderable)
            continue;

        auto& renderable = *cmd.renderable;
        renderable.destroyBuffers(VAOManager);
        m_memoryManager.destroy(renderable);
    }

    if (m_sceneManager)
    {
        m_sceneManager->getRootSceneNode(Ogre::SCENE_STATIC)->detachObject(m_dummyMovableObject);
        delete m_dummyMovableObject;
    }
    delete m_commandBuffer;
}

void RenderInterface::OnResourcesLoaded()
{
    auto* renderSystem = Ogre::Root::getSingleton().getRenderSystem();
    VAOManager     = renderSystem->getVaoManager();
    TextureManager = renderSystem->getTextureGpuManager();

    m_drawCommands.reserve(256);
    m_garbageRenderables.reserve(64);
    m_memoryManager.registerPool<Renderable, 256, 128>();
    m_transforms.reserve(128);

    createBlankMaterial();
    createBaseMaterial();
    createMaskMaterial();

    AddShaderMaker("linear-gradient", std::make_unique<LinearGradientMaker>());
    AddShaderMaker("radial-gradient", std::make_unique<RadialGradientMaker>());
    AddShaderMaker("conic-gradient",  std::make_unique<ConicGradientMaker >());

    AddFilterMaker("blur",        std::make_unique<BlurFilterMaker      >());
    AddFilterMaker("drop-shadow", std::make_unique<DropShadowFilterMaker>());
    AddFilterMaker("opacity",     std::make_unique<OpacityFilterMaker   >());
    AddFilterMaker("brightness",  std::make_unique<BrightnessFilterMaker>());
    AddFilterMaker("contrast",    std::make_unique<ContrastFilterMaker  >());
    AddFilterMaker("invert",      std::make_unique<InvertFilterMaker    >());
    AddFilterMaker("grayscale",   std::make_unique<GrayscaleFilterMaker >());
    AddFilterMaker("sepia",       std::make_unique<SepiaFilterMaker     >());
    AddFilterMaker("hue-rotate",  std::make_unique<HueRotateFilterMaker >());
    AddFilterMaker("saturate",    std::make_unique<SaturateFilterMaker  >());
}

void RenderInterface::createBaseMaterial()
{
    m_baseMaterial = Ogre::MaterialManager::getSingleton().create("!!OgreRmlUi_BaseMat",
        Ogre::ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME);

    Ogre::Pass *pass = m_baseMaterial->getTechnique(0)->getPass(0);
    pass->setVertexProgram("Rml/Element_vs");
    pass->setFragmentProgram("Rml/Base_ps");

    pass->setSamplerblock(m_samplerblock);
    pass->setBlendblock(m_blendblock);
    pass->setMacroblock(m_macroblock);

    pass->createTextureUnitState();
}

void RenderInterface::createBlankMaterial()
{
    m_blankMaterial = Ogre::MaterialManager::getSingleton().create("!!OgreRmlUi_BlankMat",
        Ogre::ResourceGroupManager::INTERNAL_RESOURCE_GROUP_NAME);

    auto* pass = m_blankMaterial->getTechnique(0)->getPass(0);
    pass->setVertexProgram("Rml/Element_vs");
    pass->setFragmentProgram("Rml/Blank_ps");

    pass->setSamplerblock(m_samplerblock);
    pass->setBlendblock(m_blendblock);
    pass->setMacroblock(m_macroblock);
}

void RenderInterface::createMaskMaterial()
{
    assert(m_blankMaterial);
    m_maskMaterial = m_blankMaterial->clone("!!OgreRmlUi_MaskMat");

    Ogre::HlmsBlendblock maskBlendblock = m_blendblock;
    maskBlendblock.mBlendChannelMask = 0;

    auto* pass = m_maskMaterial->getTechnique(0)->getPass(0);
    pass->setBlendblock(maskBlendblock);
}

Ogre::TextureGpu* RenderInterface::acquireLayerTexture(float vpWidth, float vpHeight)
{
    auto& texture = m_rttPool[m_layerIndexRef];
    if (texture)
        return texture;

    texture = TextureManager->createTexture("!!OgreRmlUi_LayerTex_" + Ogre::StringConverter::toString(Ogre::Id::generateNewId<Ogre::TextureGpu>()),
        Ogre::GpuPageOutStrategy::Discard,
        Ogre::TextureFlags::RenderToTexture,
        Ogre::TextureTypes::Type2D);
    texture->setNumMipmaps(1);
    texture->setResolution((uint32)std::ceil(vpWidth), (uint32)std::ceil(vpHeight));
    texture->setPixelFormat(Ogre::PixelFormatGpu::PFG_RGBA8_UNORM);
    texture->_transitionTo(Ogre::GpuResidency::Resident, nullptr);
    texture->_setNextResidencyStatus(Ogre::GpuResidency::Resident);
    return texture;
}

Ogre::Matrix4 RenderInterface::getProjectionMatrix( Ogre::RenderSystem* rs, const bool bRequiresTextureFlipping,
                                           const Ogre::Camera* currentCamera, float vpWidth, float vpHeight ) const
{
    Ogre::Matrix4 projectionMatrix{ 2.0f / vpWidth,  0.0f           ,  0.0f, -1.0f,
                                    0.0f          , -2.0f / vpHeight,  0.0f,  1.0f,
                                    0.0f          ,  0.0f           , -1.0f,  0.0f,
                                    0.0f          ,  0.0f           ,  0.0f,  1.0f };
    // Still need to take RS depth into account.
//    rs->_convertProjectionMatrix( projectionMatrix, projectionMatrix );
//#if OGRE_NO_VIEWPORT_ORIENTATIONMODE == 0
//    projectionMatrix = projectionMatrix * Quaternion(currentCamera->getOrientationModeAngle(), Vector3::UNIT_Z);
//#endif
//
//    if (bRequiresTextureFlipping)
//    {
//        // Invert transformed y.
//        projectionMatrix[1][0] = -projectionMatrix[1][0];
//        projectionMatrix[1][1] = -projectionMatrix[1][1];
//        projectionMatrix[1][2] = -projectionMatrix[1][2];
//        projectionMatrix[1][3] = -projectionMatrix[1][3];
//    }
    return projectionMatrix;
}

void RenderInterface::injectDatablock(Renderable& renderable, Ogre::TextureGpu* texture)
{
    auto* hlmsManager = Ogre::Root::getSingleton().getHlmsManager();
    auto* hlmsUnlit   = static_cast<Ogre::HlmsUnlit*>(hlmsManager->getHlms(Ogre::HLMS_UNLIT));
    auto* datablock   = static_cast<Ogre::HlmsUnlitDatablock*>(hlmsUnlit->createDatablock("", "", m_macroblock, m_blendblock, {}));
    datablock->setUseColour(true);
    if (texture)
        datablock->setTexture(0, texture, &m_samplerblock);
}

void RenderInterface::drawIntoCompositor(Ogre::RenderPassDescriptor* renderPassDesc, Ogre::TextureGpu* anyTargetTexture, Ogre::Camera* currentCamera)
{
    auto*      renderSystem            = m_sceneManager->getDestinationRenderSystem();
    const bool supportsIndirectBuffers = VAOManager->supportsIndirectBuffers();
    const auto numNeededDraws          = m_drawCommands.size();
    const bool bWasReadyForPresent     = renderPassDesc->mReadyWindowForPresent;
    const auto viewportSize            = Ogre::Vector4( 0, 0, 1, 1 );
    Ogre::RenderingMetrics stats;

    unsigned char *indirectDraw = 0;
    if (numNeededDraws > 0)
    {
        renderPassDesc->mReadyWindowForPresent = false;

        if (!m_indirectBuffer || (numNeededDraws * sizeof(Ogre::CbDrawIndexed)) > m_indirectBuffer->getNumElements())
        {
            if (m_indirectBuffer)
            {
                if (m_indirectBuffer->getMappingState() != Ogre::MS_UNMAPPED)
                {
                    m_indirectBuffer->unmap(Ogre::UO_UNMAP_ALL);
                }
                VAOManager->destroyIndirectBuffer( m_indirectBuffer );
            }
            m_indirectBuffer = VAOManager->createIndirectBuffer(numNeededDraws * sizeof(Ogre::CbDrawIndexed),
                                                                Ogre::BT_DYNAMIC_PERSISTENT, 0, false);
        }

        if (supportsIndirectBuffers)
            indirectDraw = static_cast<unsigned char*>(m_indirectBuffer->map(0, m_indirectBuffer->getNumElements()));
        else
            indirectDraw = m_indirectBuffer->getSwBufferPtr();

        for (size_t i = 0; i < numNeededDraws; ++i)
        {
            auto& cmd        = m_drawCommands[i];
            auto* renderable = cmd.renderable;
            if (!renderable)
                continue;

            auto* vao = renderable->getVaos(Ogre::VpNormal)[0];

            auto& cbCmd = *reinterpret_cast<Ogre::CbDrawIndexed*>(indirectDraw);
            indirectDraw += sizeof(Ogre::CbDrawIndexed);
            
            cbCmd.primCount        = vao->getPrimitiveCount();
            cbCmd.instanceCount    = 1;
            cbCmd.firstVertexIndex = uint32(vao->getIndexBuffer()->_getFinalBufferStart() + vao->getPrimitiveStart());
            cbCmd.baseVertex       = uint32(vao->getBaseVertexBuffer()->_getFinalBufferStart());
            cbCmd.baseInstance     = 0;
        }

        if (indirectDraw && supportsIndirectBuffers)
            m_indirectBuffer->unmap(Ogre::UO_KEEP_PERSISTENT);
    }

    auto* hlmsManager = Ogre::Root::getSingleton().getHlmsManager();
    auto* hlms        = hlmsManager->getHlms(Ogre::HLMS_LOW_LEVEL);

    m_commandBuffer->setCurrentRenderSystem( renderSystem );
    size_t skippedPasses = 0;

    int baseInstanceAndIndirectBuffers = 0;
    if (VAOManager->supportsIndirectBuffers())
        baseInstanceAndIndirectBuffers = 2;
    else if (VAOManager->supportsBaseInstance())
        baseInstanceAndIndirectBuffers = 1;

    const float vpWidth    = float( anyTargetTexture->getWidth() );
    const float vpHeight   = float( anyTargetTexture->getHeight() );
    const auto  projMatrix = getProjectionMatrix(renderSystem, renderPassDesc->requiresTextureFlipping(), currentCamera, vpWidth, vpHeight);
    //m_camera->setOrthoWindow(vpWidth, vpHeight);
    auto translationMatrix = Ogre::Matrix4::IDENTITY;

    m_layerIndexRef = 0;
    m_rttPool.resize(m_layerIndexMax + 1);

    auto lastType           = DrawCommand::Type::Geometry;
    auto lastScissors       = viewportSize;
    auto lastScissorEnabled = false;
    auto lastStencilValue   = uint16(0);
    auto lastClipMaskOp     = ClipMaskOperation::None;
    auto lastTransformIdx   = UINT16_MAX;

    const bool wasCustomView = currentCamera->isCustomViewMatrixEnabled();
    const bool wasCustomProj = currentCamera->isCustomProjectionMatrixEnabled();

    //currentCamera->setCustomViewMatrix(true, Ogre::Matrix4::IDENTITY);
    //currentCamera->setProjectionType(Ogre::PT_ORTHOGRAPHIC);

    Ogre::HlmsCache const* hlmsCache = nullptr;

    size_t indirectIdx = 0;
    for (size_t i = 0; i < numNeededDraws; i++)
    {
        auto& cmd        = m_drawCommands[i];
        auto* renderable = cmd.renderable;
        if (!renderable)
        {
            switch (cmd.type)
            {
            case DrawCommand::Type::PushLayer:
            {
                if (i != 0)
                {
                    int flags = Ogre::RenderPassDescriptor::Colour;
                    if (lastClipMaskOp != ClipMaskOperation::None)
                    {
                        flags |= Ogre::RenderPassDescriptor::Stencil;
                    }
                    renderPassDesc->entriesModified(flags);
                    renderSystem->endRenderPassDescriptor();
                }

                auto* layerTex  = acquireLayerTexture(vpWidth, vpHeight);
                auto* layerDesc = renderSystem->createRenderPassDescriptor();
                auto& colourBuf = layerDesc->mColour[0];
                colourBuf.texture     = layerTex;
                colourBuf.loadAction  = Ogre::LoadAction::Clear;
                colourBuf.storeAction = Ogre::StoreAction::Store;
                colourBuf.clearColour = Ogre::ColourValue::ZERO;
                layerDesc->entriesModified(Ogre::RenderPassDescriptor::Colour);

                renderSystem->beginRenderPassDescriptor(layerDesc, layerTex, 0, &viewportSize, &viewportSize, 1, false, false);
                renderSystem->executeRenderPassDescriptorDelayedActions();
                m_renderStack.push_back({ layerDesc, layerTex });
            }
            break;

            case DrawCommand::Type::PopLayer:
            {
                renderSystem->endRenderPassDescriptor();

                m_renderStack.pop_back();
                //auto& context = m_renderStack.back();

                //renderSystem->beginRenderPassDescriptor(context.passDesc, context.target, 0, &viewportSize, &viewportSize, 1, false, false);
                //renderSystem->executeRenderPassDescriptorDelayedActions();
            }
            break;

            case DrawCommand::Type::CompositeLayers:
            {
                //auto& dstContext = m_renderStack[cmd.destLayerIndex];
                //auto& srcContext = m_renderStack[cmd.srcLayerIndex];

                //renderSystem->beginRenderPassDescriptor(context.passDesc, context.target, 0, &viewportSize, &viewportSize, 1, false, false);
                //renderSystem->executeRenderPassDescriptorDelayedActions();
            }
            break;

            default:
                assert(false);
            break;
            }

            lastType = cmd.type;
            continue;
        }

        auto* vao = renderable->getVaos(Ogre::VpNormal)[0];
        OGRE_ASSERT_MEDIUM( vao->getVaoName() != 0 &&
                    "Invalid Vao name! This can happen if a BT_IMMUTABLE buffer was "
                    "recently created and VaoRenderInterface::_beginFrame() wasn't called" );

        auto* pass = renderable->getMaterial()->getTechnique(0)->getPass(0);

        Ogre::Vector4 scissors = viewportSize;
        if (cmd.scissorEnabled)
        {
            auto scLeft   = Ogre::Math::Clamp(cmd.scissor.x, 0.0f, vpWidth );
            auto scTop    = Ogre::Math::Clamp(cmd.scissor.y, 0.0f, vpHeight);
            auto scRight  = Ogre::Math::Clamp(cmd.scissor.z, 0.0f, vpWidth );
            auto scBottom = Ogre::Math::Clamp(cmd.scissor.w, 0.0f, vpHeight);

            auto left   = scLeft / vpWidth;
            auto top    = scTop / vpHeight;
            auto width  = (scRight - scLeft) / vpWidth;
            auto height = (scBottom - scTop) / vpHeight;
            scissors = Ogre::Vector4(left, top, width, height);
        }

        Ogre::StencilParams stencilParams;
        if (cmd.type == DrawCommand::Type::ClipMask)
        {
            stencilParams.enabled = true;

            if (cmd.clipMaskOp == ClipMaskOperation::Set || cmd.clipMaskOp == ClipMaskOperation::SetInverse)
            {
                stencilParams.stencilFront.compareOp     = Ogre::CMPF_ALWAYS_PASS;
                stencilParams.stencilFront.stencilPassOp = Ogre::SOP_REPLACE;
                stencilParams.stencilBack                = stencilParams.stencilFront;
                renderSystem->setStencilBufferParams(cmd.stencilValue, stencilParams);
            }
            else if (cmd.clipMaskOp == ClipMaskOperation::Intersect)
            {
                stencilParams.stencilFront.compareOp     = Ogre::CMPF_EQUAL;
                stencilParams.stencilFront.stencilPassOp = Ogre::SOP_INCREMENT;
                stencilParams.stencilBack                = stencilParams.stencilFront;
                // Previous
                renderSystem->setStencilBufferParams(cmd.stencilValue - 1, stencilParams);
            }
        }
        else if (cmd.type == DrawCommand::Type::Geometry)
        {
            if (cmd.clipMaskOp != ClipMaskOperation::None)
            {
                stencilParams.enabled                    = true;
                stencilParams.stencilFront.stencilPassOp = Ogre::SOP_KEEP;
                stencilParams.stencilFront.compareOp     = cmd.clipMaskOp == ClipMaskOperation::SetInverse ? Ogre::CMPF_NOT_EQUAL : Ogre::CMPF_EQUAL;
                stencilParams.stencilBack                = stencilParams.stencilFront;
                renderSystem->setStencilBufferParams(cmd.stencilValue, stencilParams);
            }
            else
            {
                stencilParams.enabled = false;
                renderSystem->setStencilBufferParams(0, stencilParams);
            }
        }

        if (cmd.texture)
        {
            auto* textureUnit = pass->getTextureUnitState(0);
            textureUnit->setTexture(cmd.texture);
        }

        translationMatrix.setTrans(Ogre::Vector3(cmd.translation.x, cmd.translation.y, 0));
        auto finalProjMatrix = (cmd.transformIndex == UINT16_MAX)
            ? projMatrix * translationMatrix
            : projMatrix * m_transforms[cmd.transformIndex] * translationMatrix;
        //pass->getVertexProgramParameters()->setNamedConstant("ProjectionMatrix", finalProjMatrix);

        Ogre::QueuedRenderable queuedRenderable(0, renderable, m_dummyMovableObject);

        if (i == 0 || lastType != cmd.type || lastScissorEnabled != cmd.scissorEnabled || lastTransformIdx != cmd.transformIndex
            || lastStencilValue != cmd.stencilValue || lastClipMaskOp != cmd.clipMaskOp || lastScissors != scissors)
        {
            if (i != 0)
            {
                int flags = Ogre::RenderPassDescriptor::Colour;
                if (lastClipMaskOp != ClipMaskOperation::None)
                {
                    flags |= Ogre::RenderPassDescriptor::Stencil;
                }
                renderPassDesc->entriesModified(flags);
                renderSystem->endRenderPassDescriptor();
            }
            lastType           = cmd.type;
            lastScissors       = scissors;
            lastScissorEnabled = cmd.scissorEnabled;
            lastStencilValue   = cmd.stencilValue;
            lastClipMaskOp     = cmd.clipMaskOp;
            lastTransformIdx   = cmd.transformIndex;

            currentCamera->setCustomViewMatrix(true, Ogre::Matrix4::IDENTITY);
            currentCamera->setCustomProjectionMatrix(true, finalProjMatrix);
            auto passCache = hlms->preparePassHash(0, false, false, m_sceneManager);
            hlmsCache = hlms->getMaterial( &c_dummyCache, passCache, queuedRenderable, false, nullptr );

            renderSystem->beginRenderPassDescriptor(renderPassDesc, anyTargetTexture, 0, &viewportSize, &scissors, 1, false, false);
            renderSystem->executeRenderPassDescriptorDelayedActions();
        }
        else
            skippedPasses++;

        auto* psoCmd    = m_commandBuffer->addCommand<Ogre::CbPipelineStateObject>();
        *psoCmd = Ogre::CbPipelineStateObject(&hlmsCache->pso);
        hlms->fillBuffersForV2(hlmsCache, queuedRenderable, false, 0u, m_commandBuffer);

        *m_commandBuffer->addCommand<Ogre::CbVao           >() = Ogre::CbVao(vao);
        *m_commandBuffer->addCommand<Ogre::CbIndirectBuffer>() = Ogre::CbIndirectBuffer(m_indirectBuffer);

        void* offset = reinterpret_cast<void*>(m_indirectBuffer->_getFinalBufferStart() + sizeof(Ogre::CbDrawIndexed) * indirectIdx);
        Ogre::CbDrawCallIndexed *drawCall = m_commandBuffer->addCommand<Ogre::CbDrawCallIndexed>();
        *drawCall = Ogre::CbDrawCallIndexed( baseInstanceAndIndirectBuffers, vao, offset );
        drawCall->numDraws = 1;

        stats.mDrawCount     += 1;
        stats.mInstanceCount += 1;
        stats.mFaceCount     += vao->getPrimitiveCount() / 3;
        stats.mVertexCount   += vao->getPrimitiveCount();

        hlms->preCommandBufferExecution(m_commandBuffer);
        m_commandBuffer->execute();
        hlms->postCommandBufferExecution(m_commandBuffer);

        indirectIdx++;
    }

    if (numNeededDraws > 0)
    {
        renderPassDesc->mReadyWindowForPresent = true;
        renderPassDesc->entriesModified(Ogre::RenderPassDescriptor::Colour);
        renderSystem->endRenderPassDescriptor();
        if (!wasCustomView)
            currentCamera->setCustomViewMatrix(false, Ogre::Matrix4::IDENTITY);
        if (!wasCustomProj)
            currentCamera->setCustomProjectionMatrix(false, Ogre::Matrix4::IDENTITY);
    }
    else if (bWasReadyForPresent && !stats.mDrawCount) // There was nothing for RmlUi to draw. We must still prepare the window for presenting.
    {
        renderSystem->beginRenderPassDescriptor(renderPassDesc, anyTargetTexture, 0, &viewportSize, &viewportSize, 1, false, false);
        renderSystem->executeRenderPassDescriptorDelayedActions();
        renderSystem->endRenderPassDescriptor();
    }

    renderSystem->_addMetrics(stats);
}

void RenderInterface::addDrawCommand(DrawCommand const& command)
{
    m_drawCommands.push_back(command);
}

void RenderInterface::injectNewRenderable(DrawCommand& command, Ogre::MaterialPtr const& material)
{
    auto& renderable = m_memoryManager.create<Renderable>();
    renderable.shareSameVAO(*command.renderable);
    if (material)
    {
        renderable.setMaterial(material);
        return;
    }

    if (renderable.getMaterial())
        return;

    renderable.setMaterial(command.texture ? m_baseMaterial : m_blankMaterial);
}

DrawCommand* RenderInterface::getLastDrawCommand()
{
    if (m_drawCommands.empty())
        return nullptr;

    return &m_drawCommands.back();
}

void RenderInterface::releaseTexture(Ogre::TextureGpu* texture)
{
    if (texture)
        TextureManager->destroyTexture(texture);
}

void RenderInterface::AddShaderMaker(std::string_view name, std::unique_ptr<ShaderMaker>&& maker)
{
    m_shaderMakers[StringHasher(name)] = std::move(maker);
}

void RenderInterface::AddFilterMaker(std::string_view name, std::unique_ptr<FilterMaker>&& maker)
{
    m_filterMakers[StringHasher(name)] = std::move(maker);
}

void RenderInterface::setSceneManager(Ogre::SceneManager* sceneManager)
{
    if (m_sceneManager == sceneManager)
        return;

    if (m_dummyMovableObject && m_sceneManager)
    {
        auto* rootNode = m_sceneManager->getRootSceneNode(Ogre::SCENE_STATIC);
        rootNode->detachObject(m_dummyMovableObject);
        //rootNode->detachObject(m_camera);
        delete m_dummyMovableObject;
        //delete m_camera;
    }

    m_sceneManager = sceneManager;
    m_dummyMovableObject = OGRE_NEW RmlUiDummyMO(
        Ogre::Id::generateNewId<Ogre::MovableObject>(),
        &sceneManager->_getEntityMemoryManager(Ogre::SCENE_STATIC), sceneManager, 254);
    auto* rootNode = m_sceneManager->getRootSceneNode(Ogre::SCENE_STATIC);
    rootNode->attachObject(m_dummyMovableObject);
    m_dummyMovableObject->setVisible(false);
    m_dummyMovableObject->setCastShadows(false);

    //m_camera = m_sceneManager->createCamera("!!OgreRmlUi_Camera");
    //rootNode->attachObject(m_camera);
    //m_camera->setAutoAspectRatio(true);
    //m_camera->setNearClipDistance(0.0f);
    //m_camera->setFarClipDistance(1000.0f);
    //m_camera->setUseIdentityView(true);
    //m_camera->setProjectionType(Ogre::ProjectionType::PT_ORTHOGRAPHIC);
}

void RenderInterface::BeginFrame()
{
    for (auto* renderable : m_garbageRenderables)
    {
        renderable->destroyBuffers(VAOManager);
        m_memoryManager.destroy(*renderable);
    }
    m_garbageRenderables.clear();
    for (auto id : m_garbageFilterIds)
    {
        auto entry = m_filters.find(id);
        if (entry == m_filters.end())
            return;

        auto& filter = *entry->second;
        filter.release(*this);
        m_filters.erase(entry);
    }
    m_garbageFilterIds.clear();
    m_drawCommands.clear();
    m_scissorRef = { 0.0f, 0.0f, 1.0f, 1.0f };
    m_scissorEnabled = false;
    m_clipMaskEnabled = false;
    m_stencilRefValue = 0;
    m_stencilBaseValue = 0;
    m_transformRefIndex = UINT16_MAX;
    m_transforms.clear();
}

 void RenderInterface::EndFrame()
 {
     // Nothing
 }

Rml::CompiledGeometryHandle RenderInterface::CompileGeometry(
    Rml::Span<const Rml::Vertex> vertices,
    Rml::Span<const int> indices)
{
    auto& renderable = m_memoryManager.create<Renderable>();
    renderable.updateVertexData(vertices, indices, VAOManager);

    return reinterpret_cast<Rml::CompiledGeometryHandle>(&renderable);
}

void RenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry)
{
    auto* renderable = reinterpret_cast<Renderable*>(geometry);
    m_garbageRenderables.push_back(renderable);
}

void RenderInterface::RenderGeometry(
        Rml::CompiledGeometryHandle geometry,
        Rml::Vector2f translation,
        Rml::TextureHandle texture) 
{
    auto& cmd = m_drawCommands.emplace_back();
    //cmd.id              = ++m_cmdIdCounter;
    cmd.renderable      = reinterpret_cast<Renderable*>(geometry);
    cmd.texture         = reinterpret_cast<Ogre::TextureGpu*>(texture);
    cmd.scissor         = m_scissorRef;
    cmd.scissorEnabled  = m_scissorEnabled;
    cmd.type            = DrawCommand::Type::Geometry;
    if (m_clipMaskEnabled)
    {
        cmd.stencilValue = m_stencilRefValue;
        cmd.clipMaskOp   = m_clipMaskOpRef;
    }
    else
    {
        cmd.clipMaskOp   = ClipMaskOperation::None;
    }
    cmd.translation     = translation;
    cmd.transformIndex  = m_transformRefIndex;

    cmd.renderable->setMaterial(texture ? m_baseMaterial : m_blankMaterial);
}

void RenderInterface::EnableScissorRegion(bool enable)
{
    m_scissorEnabled = enable;
}

void RenderInterface::SetScissorRegion(Rml::Rectanglei region)
{
    m_scissorRef = Ogre::Vector4{ (float)region.Left(), (float)region.Top(), (float)region.Right(), (float)region.Bottom() };
}

void RenderInterface::SetTransform(const Rml::Matrix4f* transform)
{
    if (!transform)
    {
        m_transformRefIndex = UINT16_MAX;
        return;
    }

    m_transforms.emplace_back(Ogre::Matrix4(transform->data()).transpose());
    m_transformRefIndex = uint16(m_transforms.size() - 1);
}

Rml::TextureHandle RenderInterface::LoadTexture(
    Rml::Vector2i& texture_dimensions,
    const Rml::String& source
)
{
    return 0;
}

Rml::TextureHandle RenderInterface::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions)
{  
    Ogre::String texName = "RmlUiTex_" + Ogre::StringConverter::toString(Ogre::Id::generateNewId<Ogre::TextureGpu>());
    Ogre::uint8* data = reinterpret_cast<Ogre::uint8*>(
        OGRE_MALLOC_SIMD(source.size(), Ogre::MEMCATEGORY_GENERAL));
    std::copy(source.begin(), source.end(), data);

    auto* image = OGRE_NEW Ogre::Image2;
    image->loadDynamicImage(data,
        source_dimensions.x, source_dimensions.y, 1,
        Ogre::TextureTypes::Type2D,
        Ogre::PixelFormatGpu::PFG_RGBA8_UNORM,
        true,
        1);

    Ogre::TextureGpu* texture = TextureManager->createTexture(
        texName,
        Ogre::GpuPageOutStrategy::Discard,
        0,
        Ogre::TextureTypes::Type2D,
        Ogre::BLANKSTRING);
    texture->setNumMipmaps(1);
    texture->setResolution(uint32(source_dimensions.x), uint32(source_dimensions.y));
    texture->setPixelFormat(Ogre::PixelFormatGpu::PFG_RGBA8_UNORM);
    texture->scheduleTransitionTo(Ogre::GpuResidency::Resident, image);
 
    return reinterpret_cast<Rml::TextureHandle>(texture);
}

void RenderInterface::ReleaseTexture(Rml::TextureHandle texture_handle)
{
    auto* tex = reinterpret_cast<Ogre::TextureGpu*>(texture_handle);
    if (tex)
        TextureManager->destroyTexture(tex);
}

void RenderInterface::EnableClipMask(bool enable)
{
    m_clipMaskEnabled = enable;
}

void RenderInterface::RenderToClipMask(
    Rml::ClipMaskOperation operation,
    Rml::CompiledGeometryHandle geometry,
    Rml::Vector2f translation)
{
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

    auto& cmd = m_drawCommands.emplace_back();
    cmd.renderable      = reinterpret_cast<Renderable*>(geometry);
    cmd.texture         = nullptr;
    cmd.scissor         = m_scissorRef;
    cmd.scissorEnabled  = m_scissorEnabled;
    cmd.type            = DrawCommand::Type::ClipMask;
    cmd.stencilValue    = m_stencilRefValue;
    cmd.clipMaskOp      = m_clipMaskOpRef;
    cmd.translation     = translation;
    cmd.transformIndex  = m_transformRefIndex;

    cmd.renderable->setMaterial(m_maskMaterial);
}

Rml::LayerHandle RenderInterface::PushLayer()
{
    auto& cmd = m_drawCommands.emplace_back();
    cmd.type = DrawCommand::Type::PushLayer;
    m_layerIndexMax = std::max(m_layerIndexMax, m_layerIndexRef);
    return ++m_layerIndexRef;
}

void RenderInterface::CompositeLayers(
    Rml::LayerHandle source,
    Rml::LayerHandle destination,
    Rml::BlendMode blend_mode,
    Rml::Span<const Rml::CompiledFilterHandle> filters)
{
    auto& cmd = m_drawCommands.emplace_back();
    cmd.type           = DrawCommand::Type::CompositeLayers;
    cmd.srcLayerIndex  = uint16(--source);
    cmd.destLayerIndex = uint16(--destination);
    cmd.blendMode      = BlendMode(blend_mode);
    if (!filters.empty())
    {
        cmd.filterSetIndex = (uint16)m_filterSets.size();
        auto& filterSet = m_filterSets.emplace_back();
        filterSet.reserve(filters.size());
        for (auto filter : filters)
            filterSet.push_back(filter);
    }
    else
        cmd.filterSetIndex = UINT16_MAX;
}

void RenderInterface::PopLayer()
{
    auto& cmd = m_drawCommands.emplace_back();
    cmd.type = DrawCommand::Type::PopLayer;
    m_layerIndexRef--;
}

Rml::TextureHandle RenderInterface::SaveLayerAsTexture()
{
    return {};
}

Rml::CompiledFilterHandle RenderInterface::SaveLayerAsMaskImage()
{
    return {};
}

Rml::CompiledFilterHandle RenderInterface::CompileFilter(
    const Rml::String& name,
    const Rml::Dictionary& parameters)
{
    auto hash = StringHasher(name);

    auto entry = m_filterMakers.find(hash);
    if (entry == m_filterMakers.end())
        throw std::runtime_error("OgreRmlUi: FilterMaker with name '" + name + "' not found");

    auto& filterMaker = entry->second;
    m_filters.emplace(++m_shaderFilterIdCounter, filterMaker->make(parameters));
    return m_shaderFilterIdCounter;
}

void RenderInterface::ReleaseFilter(Rml::CompiledFilterHandle filter)
{
    m_garbageFilterIds.insert(filter);
}

Rml::CompiledShaderHandle RenderInterface::CompileShader(
    const Rml::String& name,
    const Rml::Dictionary& parameters)
{
    auto hash = StringHasher(name);

    auto entry = m_shaderMakers.find(hash);
    if (entry == m_shaderMakers.end())
        throw std::runtime_error("OgreRmlUi: ShaderMaker with name '" + name + "' not found");

    auto& shaderMaker = entry->second;
    m_shaderMaterials.emplace(++m_shaderFilterIdCounter, shaderMaker->make(parameters));
    return m_shaderFilterIdCounter;
}

void RenderInterface::RenderShader(
    Rml::CompiledShaderHandle shader,
    Rml::CompiledGeometryHandle geometry,
    Rml::Vector2f translation,
    Rml::TextureHandle texture)
{
    if (texture)
        throw std::runtime_error("OgreRmlUi: RenderShader does not support texture. Set the texture from your ShaderMaker");

    if (!shader)
        throw std::runtime_error("OgreRmlUi: RenderShader called with null shader");

    auto index = static_cast<uint16>(geometry) - 1;

    auto& cmd = m_drawCommands.emplace_back();
    cmd.renderable      = reinterpret_cast<Renderable*>(geometry);
    cmd.texture         = nullptr;
    cmd.scissor         = m_scissorRef;
    cmd.scissorEnabled  = m_scissorEnabled;
    cmd.type            = DrawCommand::Type::Geometry;
    if (m_clipMaskEnabled)
    {
        cmd.stencilValue = m_stencilRefValue;
        cmd.clipMaskOp   = m_clipMaskOpRef;
    }
    else
    {
        cmd.clipMaskOp   = ClipMaskOperation::None;
    }
    cmd.translation     = translation;
    cmd.transformIndex  = m_transformRefIndex;

    auto& mat = m_shaderMaterials.at(shader);
    cmd.renderable->setMaterial(mat);
}

void RenderInterface::ReleaseShader(Rml::CompiledShaderHandle shader)
{
    m_shaderMaterials.erase(shader);
}
