#include <OgreRmlUi/CompositorPass.h>

#include <Compositor/Pass/OgreCompositorPass.h>
#include <Compositor/Pass/OgreCompositorPassDef.h>
#include "Compositor/OgreCompositorNode.h"
#include "Compositor/OgreTextureDefinition.h"
#include "OgreCamera.h"
#include "OgrePixelFormatGpuUtils.h"
#include "OgreSceneManager.h"

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
                       Ogre::CompositorNode* parentNode, RenderInterface* Manager);

        void execute(const Ogre::Camera* lodCamera) final;

    private:
        CompositorPassDef const* m_definition;
        Ogre::SceneManager*      m_sceneManager;
        Ogre::Camera*            m_camera;
        RenderInterface*         m_manager;
        Ogre::RenderTargetViewDef const* m_rtv;
    };
}

CompositorPassProvider::CompositorPassProvider( RenderInterface *Manager ) :
    mManager( Manager )
{
}
//-------------------------------------------------------------------------
Ogre::CompositorPassDef *CompositorPassProvider::addPassDef( Ogre::CompositorPassType passType,
                                                            Ogre::IdString customId,
                                                            Ogre::CompositorTargetDef *parentTargetDef,
                                                            Ogre::CompositorNodeDef *parentNodeDef )
{
    if (customId == "rmlui")
        return OGRE_NEW CompositorPassDef( parentTargetDef );

    return 0;
}
//-------------------------------------------------------------------------
Ogre::CompositorPass *CompositorPassProvider::addPass( const Ogre::CompositorPassDef *definition,
                                                        Ogre::Camera* defaultCamera,
                                                        Ogre::CompositorNode *parentNode,
                                                        const Ogre::RenderTargetViewDef *rtvDef,
                                                        Ogre::SceneManager *sceneManager )
{
    // Not created by us.
    if( definition->getCustomId() != Ogre::IdString("rmlui").getU32Value() )
        return 0;

    OGRE_ASSERT_HIGH( dynamic_cast<const CompositorPassDef *>( definition ) );
    auto* rmluiDef = static_cast<const CompositorPassDef *>( definition );
    return OGRE_NEW CompositorPass( rmluiDef, defaultCamera, sceneManager, rtvDef, parentNode, mManager );
}

CompositorPass::CompositorPass(CompositorPassDef const* definition, Ogre::Camera* defaultCamera,
                               Ogre::SceneManager* sceneManager, Ogre::RenderTargetViewDef const* rtv,
                               Ogre::CompositorNode* parentNode, RenderInterface* Manager) :
    Ogre::CompositorPass(definition, parentNode),
    m_sceneManager      (sceneManager),
    m_camera            (defaultCamera),
    m_manager           (Manager),
    m_definition        (definition),
    m_rtv               (rtv)
{
    initialize(rtv);
    m_manager->setSceneManager(sceneManager);
}
//-----------------------------------------------------------------------------------
void CompositorPass::execute( const Ogre::Camera* /*lodCamera*/ )
{
    // Execute a limited number of times?
    if( mNumPassesLeft != std::numeric_limits<uint32_t>::max() )
    {
        if( !mNumPassesLeft )
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

    m_manager->drawIntoCompositor(mRenderPassDesc, mAnyTargetTexture, m_camera, m_rtv);
    m_sceneManager->_setCurrentCompositorPass(0);

    notifyPassPosExecuteListeners();

    profilingEnd();
}
