#pragma once

#include "Prerequisites.h"

#include "Compositor/Pass/OgreCompositorPass.h"

#include "OgreHeaderPrefix.h"

namespace RmlOgre
{

class CompositorPassDef;

class CompositorPass : public Ogre::CompositorPass
{
protected:
    Ogre::SceneManager *mSceneManager;
    Ogre::Camera       *mCamera;
    Manager *mManager;

    Ogre::IdString mTextureName;

public:
    CompositorPass( const CompositorPassDef *definition, Ogre::Camera* defaultCamera,
                            Ogre::SceneManager *sceneManager, const Ogre::RenderTargetViewDef *rtv,
                            Ogre::CompositorNode *parentNode, Manager *Manager );

    void execute( const Ogre::Camera* lodCamera ) final;

    bool notifyRecreated( const Ogre::TextureGpu* channel ) final;

private:
    CompositorPassDef const *mDefinition;
};

}  // namespace Ogre
