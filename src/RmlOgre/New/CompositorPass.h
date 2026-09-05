#pragma once

#include "RenderInterface.h"
#include "Compositor/Pass/OgreCompositorPass.h"

namespace RmlOgre
{

class CompositorPassDef;

class CompositorPass : public Ogre::CompositorPass
{
protected:
    Ogre::SceneManager *mSceneManager;
    Ogre::Camera       *mCamera;
    RenderInterface    *mManager;

    Ogre::IdString mTextureName;

public:
    CompositorPass( const CompositorPassDef *definition, Ogre::Camera* defaultCamera,
                            Ogre::SceneManager *sceneManager, const Ogre::RenderTargetViewDef *rtv,
                            Ogre::CompositorNode *parentNode, RenderInterface *Manager );

    void execute( const Ogre::Camera* lodCamera ) final;

    bool notifyRecreated( const Ogre::TextureGpu* channel ) final;

private:
    CompositorPassDef const *mDefinition;
};

}  // namespace Ogre
