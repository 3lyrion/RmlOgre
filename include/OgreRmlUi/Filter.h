// Credits: 3lyrion [OgreRmlUi], nimble [OgreRmlUi]

#pragma once

#include <OgreRmlUi/detail/Precompiled.h>

namespace OgreRmlUi
{

class RenderInterface;

class Filter
{
public:
    virtual ~Filter() = default;
    virtual void apply(RenderInterface& renderInterface) = 0;
    virtual void release(RenderInterface& renderInterface) {}
};

class SingleMaterialFilter : public Filter
{
    Ogre::MaterialPtr material;

public:
    SingleMaterialFilter(Ogre::MaterialPtr material) :
        material{material}
    {}

    void apply(RenderInterface& renderInterface) final;
    void release(RenderInterface& renderInterface) final;
};

class FilterMaker
{
public:
    virtual ~FilterMaker() = default;

    virtual UPtr<Filter> make(Rml::Dictionary const& parameters) = 0;
};

} // namespace OgreRmlUi
