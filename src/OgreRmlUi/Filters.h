// Credits: 3lyrion [OgreRmlUi], nimble [OgreRmlUi]

#pragma once

#include <OgreRmlUi/Filter.h>

namespace OgreRmlUi
{

class BlurFilter : public Filter
{
    Ogre::MaterialPtr halfsample;
    Ogre::MaterialPtr blurH;
    Ogre::MaterialPtr blurV;
    int halfSamples;

public:
    BlurFilter(
        Ogre::MaterialPtr halfsample,
        Ogre::MaterialPtr blurH,
        Ogre::MaterialPtr blurV,
        int halfSamples
    ) :
        halfsample{halfsample},
        blurH{blurH},
        blurV{blurV},
        halfSamples{halfSamples}
    {}

    void apply(RenderInterface& renderInterface) final;
};

class BlurFilterMaker : public FilterMaker
{
    Ogre::MaterialPtr halfsampleMaterial;
    Ogre::MaterialPtr blurMaterial;

public:
    // Must be same as:
    // NUM_WEIGHTS in Rml/GLSL/Blur_ps.glsl
    // NUM_WEIGHTS in Rml/HLSL/Blur_ps.hlsl
    inline static constexpr int NUM_WEIGHTS = 8;
    inline static constexpr float MAX_SCALED_SIGMA = 3.0f;

    BlurFilterMaker();
    BlurFilter make(float sigma);
    UPtr<Filter> make(const Rml::Dictionary& parameters) final;
};

class DropShadowFilter : public Filter
{
    BlurFilter blurFilter;
    Ogre::MaterialPtr shadowMaterial;

public:
    DropShadowFilter(
        BlurFilter blurFilter,
        Ogre::MaterialPtr shadowMaterial
    ) :
        blurFilter{blurFilter},
        shadowMaterial{shadowMaterial}
    {}

    void apply(RenderInterface& renderInterface) final;
};

class DropShadowFilterMaker : public FilterMaker
{
    BlurFilterMaker blurFilterMaker;
    Ogre::MaterialPtr shadowMaterial;
    Ogre::MaterialPtr blurlessShadowMaterial;

public:
    DropShadowFilterMaker();
    UPtr<Filter> make(const Rml::Dictionary& parameters) final;
};

class OpacityFilterMaker : public FilterMaker
{
    Ogre::MaterialPtr baseMaterial;

public:
    OpacityFilterMaker();
    UPtr<Filter> make(const Rml::Dictionary& parameters) final;
};

class ColourMatrixFilterMaker : public FilterMaker
{
    Ogre::MaterialPtr baseMaterial;

public:
    ColourMatrixFilterMaker();
    using FilterMaker::make;
    UPtr<Filter> make(const Ogre::Matrix4& matrix);
};

class BrightnessFilterMaker : public ColourMatrixFilterMaker
{
public:
    using ColourMatrixFilterMaker::make;
    UPtr<Filter> make(const Rml::Dictionary& parameters) final;
};

class ContrastFilterMaker : public ColourMatrixFilterMaker
{
public:
    using ColourMatrixFilterMaker::make;
    UPtr<Filter> make(const Rml::Dictionary& parameters) final;
};

class InvertFilterMaker : public ColourMatrixFilterMaker
{
public:
    using ColourMatrixFilterMaker::make;
    UPtr<Filter> make(const Rml::Dictionary& parameters) final;
};

class GrayscaleFilterMaker : public ColourMatrixFilterMaker
{
public:
    using ColourMatrixFilterMaker::make;
    UPtr<Filter> make(const Rml::Dictionary& parameters) final;
};

class SepiaFilterMaker : public ColourMatrixFilterMaker
{
public:
    using ColourMatrixFilterMaker::make;
    UPtr<Filter> make(const Rml::Dictionary& parameters) final;
};

class HueRotateFilterMaker : public ColourMatrixFilterMaker
{
public:
    using ColourMatrixFilterMaker::make;
    UPtr<Filter> make(const Rml::Dictionary& parameters) final;
};

class SaturateFilterMaker : public ColourMatrixFilterMaker
{
public:
    using ColourMatrixFilterMaker::make;
    UPtr<Filter> make(const Rml::Dictionary& parameters) final;
};

class MaskImageFilterMaker : public FilterMaker
{
    Ogre::MaterialPtr baseMaterial;

public:
    MaskImageFilterMaker();

    SingleMaterialFilter make(Ogre::TextureGpu* image);
    UPtr<Filter> make(const Rml::Dictionary& parameters) final;
};

} // namespace OgreRmlUi
