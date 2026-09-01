
#include "CompositorPass.h"

#include "CompositorPassDef.h"
#include "Manager.h"

#include "Compositor/OgreCompositorNode.h"
#include "Compositor/OgreTextureDefinition.h"
#include "OgreCamera.h"
#include "OgrePixelFormatGpuUtils.h"
#include "OgreSceneManager.h"

using namespace RmlOgre;

CompositorPass::CompositorPass( const CompositorPassDef *definition,
                                            Ogre::Camera* defaultCamera, Ogre::SceneManager *sceneManager,
                                            const Ogre::RenderTargetViewDef *rtv, Ogre::CompositorNode *parentNode,
                                            Manager *Manager ) :
    Ogre::CompositorPass( definition, parentNode ),
    mSceneManager( sceneManager ),
    mCamera( 0 ),
    mManager( Manager ),
    mDefinition( definition )
{
    initialize( rtv );

    mCamera = defaultCamera;

    mTextureName = rtv->colourAttachments[0].textureName;
    Ogre::TextureGpu* texture = mParentNode->getDefinedTexture( mTextureName );
    //setResolutionToImgui( texture->getWidth(), texture->getHeight() );
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
//-----------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------
bool CompositorPass::notifyRecreated( const Ogre::TextureGpu* channel )
{
    bool usedByUs = CompositorPass::notifyRecreated( channel );

    //if( !usedByUs )
    //{
    //    // Because of mSkipLoadStoreSemantics, notifyRecreated may return false incorrectly.
    //    // Technically, we ignore mSkipLoadStoreSemantics so this block of code is useless.
    //    // But I'm leaving it anyway in case that changes in the future.
    //    Ogre::TextureGpu* texture = mParentNode->getDefinedTexture( mTextureName );
    //    usedByUs = texture == channel;
    //}

    //if( usedByUs &&  //
    //    !Ogre::PixelFormatGpuUtils::isDepth( channel->getPixelFormat() ) &&
    //    !Ogre::PixelFormatGpuUtils::isStencil( channel->getPixelFormat() ) )
    //{
    //    setResolutionToImgui( channel->getWidth(), channel->getHeight() );
    //}

    return usedByUs;
}
