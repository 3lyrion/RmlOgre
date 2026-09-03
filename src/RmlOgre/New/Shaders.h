#pragma once

#include <RmlUi/Core.h>
#include "Prerequisites.h"

namespace RmlOgre
{

class ShaderMaker
{
public:
    virtual ~ShaderMaker() = default;

    virtual Ogre::MaterialPtr make(Rml::Dictionary const& parameters) = 0;
};

} // namespace RmlOgre
