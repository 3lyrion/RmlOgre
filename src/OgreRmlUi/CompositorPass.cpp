#include <OgreRmlUi/CompositorPass.h>

#include <Compositor/Pass/OgreCompositorPass.h>
#include <Compositor/Pass/OgreCompositorPassDef.h>
#include <Compositor/OgreCompositorNode.h>
#include <Compositor/OgreTextureDefinition.h>
#include <OgreCamera.h>
#include <OgreSceneManager.h>

using namespace OgreRmlUi;

namespace
{
    class CompositorPassDef : public Ogre::CompositorPassDef
    {
    public:
        CompositorPassDef(Ogre::CompositorTargetDef *parentTargetDef) :
            Ogre::CompositorPassDef( Ogre::PASS_CUSTOM, parentTargetDef)
        {
            mProfilingId = "RmlUi";
        }
    };

    class CompositorPass : public Ogre::CompositorPass
    {


    public:
        CompositorPass(CompositorPassDef const* definition, Ogre::Camera* defaultCamera,
                       Ogre::SceneManager* sceneManager, Ogre::RenderTargetViewDef const* rtv,
                       Ogre::CompositorNode* parentNode, RenderInterface* Manage);

        void execute(const Ogre::Camera* lodCamera) final;

    private:
        CompositorPassDef const* m_definition;
        Ogre::SceneManager*      m_sceneManager;
        Ogre::Camera*            m_camera;
        RenderInterface*         m_manager;
    };
}

CompositorPassProvider::CompositorPassProvider(RenderInterface& manager) :
    m_manager(&manager)
{ }

Ogre::CompositorPassDef *CompositorPassProvider::addPassDef(Ogre::CompositorPassType passType, Ogre::IdString customId,
                                                            Ogre::CompositorTargetDef* parentTargetDef,
                                                            Ogre::CompositorNodeDef* parentNodeDef)
{
    if (customId == "rmlui")
        return OGRE_NEW CompositorPassDef( parentTargetDef );

    return nullptr;
}
//-------------------------------------------------------------------------
Ogre::CompositorPass* CompositorPassProvider::addPass(Ogre::CompositorPassDef const* definition,
                                                      Ogre::Camera* defaultCamera, Ogre::CompositorNode *parentNode,
                                                      Ogre::RenderTargetViewDef const* rtvDef,
                                                      Ogre::SceneManager *sceneManager)
{
    if (definition->getCustomId() != Ogre::IdString("rmlui").getU32Value())
        return nullptr;

    OGRE_ASSERT_HIGH(dynamic_cast<CompositorPassDef const*>(definition));
    auto* rmluiDef = static_cast<CompositorPassDef const*>(definition);
    return OGRE_NEW CompositorPass(rmluiDef, defaultCamera, sceneManager, rtvDef, parentNode, m_manager);
}

CompositorPass::CompositorPass(CompositorPassDef const* definition, Ogre::Camera* defaultCamera,
                               Ogre::SceneManager* sceneManager, Ogre::RenderTargetViewDef const* rtv,
                               Ogre::CompositorNode* parentNode, RenderInterface* Manager) :
    Ogre::CompositorPass(definition, parentNode),
    m_sceneManager      (sceneManager),
    m_camera            (defaultCamera),
    m_manager           (Manager),
    m_definition        (definition)
{
    initialize(rtv);
    m_manager->setSceneManager(sceneManager);
}

void CompositorPass::execute( Ogre::Camera const* /*lodCamera*/)
{
    // Execute a limited number of times?
    if (mNumPassesLeft != UINT32_MAX)
    {
        if (!mNumPassesLeft)
            return;

        --mNumPassesLeft;
    }

    profilingBegin();

    notifyPassEarlyPreExecuteListeners();

    analyzeBarriers();
    executeResourceTransitions();

    m_sceneManager->_setCamerasInProgress(Ogre::CamerasInProgress(m_camera));
    m_sceneManager->_setCurrentCompositorPass(this);

    notifyPassPreExecuteListeners();

    m_manager->drawIntoCompositor(mRenderPassDesc, mAnyTargetTexture, m_camera);
    m_sceneManager->_setCurrentCompositorPass(0);

    notifyPassPosExecuteListeners();

    profilingEnd();
}
