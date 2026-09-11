// Credits: 3lyrion [OgreRmlUi], the ogre-next team [ImGui]

#pragma once

#include <OgreRmlUi/RenderInterface.h>

#include <Compositor/Pass/OgreCompositorPassProvider.h>

namespace OgreRmlUi
{

class CompositorPassProvider : public Ogre::CompositorPassProvider
{
public:
    CompositorPassProvider(RenderInterface& manager);

    Ogre::CompositorPassDef* addPassDef(Ogre::CompositorPassType passType, Ogre::IdString customId,
                                        Ogre::CompositorTargetDef* parentTargetDef,
                                        Ogre::CompositorNodeDef* parentNodeDef) final;

    Ogre::CompositorPass* ogre_nullable addPass(Ogre::CompositorPassDef const* definition,
                                                Ogre::Camera* defaultCamera, Ogre::CompositorNode *parentNode,
                                                Ogre::RenderTargetViewDef const* rtvDef,
                                                Ogre::SceneManager *sceneManager) final;

private:
    RenderInterface* m_manager;
};

}  // namespace OgreRmlUi
