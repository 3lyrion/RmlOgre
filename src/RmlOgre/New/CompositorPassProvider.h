#pragma once

#include "Prerequisites.h"

#include "Compositor/Pass/OgreCompositorPassProvider.h"

namespace RmlOgre
{

class CompositorPassProvider : public Ogre::CompositorPassProvider
{
    Manager *mManager;

public:
    CompositorPassProvider(Manager *Manager );

    Manager *getManager() const { return mManager; }

    /** Called from CompositorTargetDef::addPass when adding a Compositor Ogre::Pass of type 'custom'
    @param passType
    @param customId
        Arbitrary ID in case there is more than one type of custom pass you want to implement.
        Defaults to IdString()
    @param rtIndex
    @param parentNodeDef
    @return
        The pass definition, or nullptr if customId is not handled by this provider.
        NOTE: The "master" provider MUST ultimately return a valid pointer.
    */
    Ogre::CompositorPassDef* addPassDef(Ogre::CompositorPassType passType, Ogre::IdString customId,
                                                    Ogre::CompositorTargetDef *parentTargetDef,
                                                    Ogre::CompositorNodeDef   *parentNodeDef ) override;

    /** Creates a CompositorPass from a CompositorPassDef for Compositor Ogre::Pass of type 'custom'
    @remarks    If you have multiple custom pass types then you will need to use dynamic_cast<>()
                on the CompositorPassDef to determine what custom pass it is.
    @return
        The pass, or nullptr if definition was not created by this provider.
        NOTE: The "master" provider MUST ultimately return a valid pointer.
    */
    Ogre::CompositorPass *ogre_nullable addPass( const Ogre::CompositorPassDef *definition,
                                            Ogre::Camera* defaultCamera, Ogre::CompositorNode *parentNode,
                                            const Ogre::RenderTargetViewDef *rtvDef,
                                            Ogre::SceneManager              *sceneManager ) override;

    void translateCustomPass( Ogre::ScriptCompiler *compiler, const Ogre::AbstractNodePtr &node,
                                Ogre::IdString customId, Ogre::CompositorPassDef *customPassDef ) override;
};

}  // namespace RmlOgre
