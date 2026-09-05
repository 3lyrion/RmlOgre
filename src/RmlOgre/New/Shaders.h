#pragma once

#include "Precompiled.h"

namespace RmlOgre
{

class ShaderMaker
{
public:
    virtual ~ShaderMaker() = default;

    virtual Ogre::MaterialPtr make(Rml::Dictionary const& parameters) = 0;
};

} // namespace RmlOgre
