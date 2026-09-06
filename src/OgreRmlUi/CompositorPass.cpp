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
        CompositorPass( const CompositorPassDef *definition, Ogre::Camera* defaultCamera,
                                Ogre::SceneManager *sceneManager, const Ogre::RenderTargetViewDef *rtv,
                                Ogre::CompositorNode *parentNode, RenderInterface *Manager );

        void execute( const Ogre::Camera* lodCamera ) final;

    private:
        CompositorPassDef const *mDefinition;
        Ogre::SceneManager      *mSceneManager;
        Ogre::Camera            *mCamera;
        RenderInterface         *mManager;
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

CompositorPass::CompositorPass( const CompositorPassDef *definition,
                                            Ogre::Camera* defaultCamera, Ogre::SceneManager *sceneManager,
                                            const Ogre::RenderTargetViewDef *rtv, Ogre::CompositorNode *parentNode,
                                            RenderInterface *Manager ) :
    Ogre::CompositorPass( definition, parentNode ),
    mSceneManager( sceneManager ),
    mCamera( defaultCamera ),
    mManager( Manager ),
    mDefinition( definition )
{
    initialize( rtv );
}
//-----------------------------------------------------------------------------------
void CompositorPass::execute( const Ogre::Camera* lodCamera )
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

    Ogre::SceneManager *sceneManager = mCamera->getSceneManager();
    sceneManager->_setCamerasInProgress( Ogre::CamerasInProgress( mCamera ) );
    sceneManager->_setCurrentCompositorPass( this );

    // Fire the listener in case it wants to change anything
    notifyPassPreExecuteListeners();

    mManager->drawIntoCompositor( mRenderPassDesc, mAnyTargetTexture, mSceneManager, mCamera );

    sceneManager->_setCurrentCompositorPass( 0 );

    notifyPassPosExecuteListeners();

    profilingEnd();
}
