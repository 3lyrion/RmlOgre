// Credits: 3lyrion [OgreRmlUi], the ogre-next team [ImGui]

#pragma once

#include <OgreRmlUi/RenderInterface.h>

#include <Compositor/Pass/OgreCompositorPassProvider.h>

namespace OgreRmlUi
{

class CompositorPassProvider : public Ogre::CompositorPassProvider
{
    RenderInterface *mManager;

public:
    CompositorPassProvider(RenderInterface *Manager );

    RenderInterface *getManager() const { return mManager; }

    Ogre::CompositorPassDef* addPassDef(Ogre::CompositorPassType passType, Ogre::IdString customId,
                                                    Ogre::CompositorTargetDef *parentTargetDef,
                                                    Ogre::CompositorNodeDef   *parentNodeDef ) final;

    Ogre::CompositorPass *ogre_nullable addPass( const Ogre::CompositorPassDef *definition,
                                            Ogre::Camera* defaultCamera, Ogre::CompositorNode *parentNode,
                                            const Ogre::RenderTargetViewDef *rtvDef,
                                            Ogre::SceneManager              *sceneManager ) final;
};

}  // namespace Ogre
