#pragma once

#include "Precompiled.h"
#include "Compositor/Pass/OgreCompositorPassDef.h"

namespace RmlOgre
{

class CompositorPassDef : public Ogre::CompositorPassDef
{
public:
    bool mSetsResolution;

public:
    CompositorPassDef( Ogre::CompositorTargetDef *parentTargetDef ) :
        Ogre::CompositorPassDef( Ogre::PASS_CUSTOM, parentTargetDef ),
        mSetsResolution( true )
    {
        mProfilingId = "RmlUi";
    }
};

}  // namespace Ogre
