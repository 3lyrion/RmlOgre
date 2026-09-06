// Credits: 3lyrion [OgreRmlUi], nimble [OgreRmlUi]

#pragma once

#include <OgreRmlUi/Shader.h>

namespace OgreRmlUi
{

class GradientMaker : public ShaderMaker
{
public:
    enum class Type
    {
        LINEAR,
        RADIAL,
        CONIC
    };

    // Must be same as:
    // MAX_STOP_COLOURS in Rml/GLSL/Gradient_ps.glsl
    // MAX_STOP_COLOURS in Rml/HLSL/Gradient_ps.hlsl
    static const int MAX_STOP_COLOURS = 16;

private:
    Type type;
    Ogre::MaterialPtr baseMaterial;

public:
    GradientMaker(Type type);
    Ogre::MaterialPtr makeGradient(
        const Rml::Dictionary& parameters,
        Ogre::Vector2& origin,
        Ogre::Vector2& scale);
};


class LinearGradientMaker : public GradientMaker
{
public:
    LinearGradientMaker() :
        GradientMaker(GradientMaker::Type::LINEAR)
    {}
    Ogre::MaterialPtr make(const Rml::Dictionary& parameters) final;
};

class RadialGradientMaker : public GradientMaker
{
public:
    RadialGradientMaker() :
        GradientMaker(GradientMaker::Type::RADIAL)
    {}
    Ogre::MaterialPtr make(const Rml::Dictionary& parameters) final;
};

class ConicGradientMaker : public GradientMaker
{
public:
    ConicGradientMaker() :
        GradientMaker(GradientMaker::Type::CONIC)
    {}
    Ogre::MaterialPtr make(const Rml::Dictionary& parameters) final;
};

}
