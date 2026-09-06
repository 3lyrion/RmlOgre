// Credits: 3lyrion [OgreRmlUi], nimble [OgreRmlUi]

#pragma once

#include <OgreRmlUi/detail/Precompiled.h>

namespace OgreRmlUi
{

class ShaderMaker
{
public:
    virtual ~ShaderMaker() = default;

    virtual Ogre::MaterialPtr make(Rml::Dictionary const& parameters) = 0;
};

} // namespace OgreRmlUi
