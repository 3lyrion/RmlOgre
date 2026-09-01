#pragma once

#include "Prerequisites.h"

#include "Compositor/Pass/OgreCompositorPassDef.h"

#include "OgreHeaderPrefix.h"

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
            mProfilingId = "Dear Imgui";
        }
    };

}  // namespace Ogre
