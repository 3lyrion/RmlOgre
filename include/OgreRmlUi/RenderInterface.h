// Credits: 3lyrion [OgreRmlUi], the ogre-next team [ImGui]

#pragma once

#include <OgreRmlUi/Filter.h>
#include <OgreRmlUi/Shader.h>
#include <OgreRmlUi/detail/MemoryManager.h>

namespace OgreRmlUi
{

class Renderable;

enum class ClipMaskOperation : int8
{
    None       = -1,
    Set        = Rml::ClipMaskOperation::Set,
    SetInverse = Rml::ClipMaskOperation::SetInverse,
    Intersect  = Rml::ClipMaskOperation::Intersect
};

enum class BlendMode : int8
{
    Blend   = Rml::BlendMode::Blend,
    Replace = Rml::BlendMode::Replace
};

struct DrawCommand
{
    enum class Type : uint8
    {
        Geometry = 0,
        ClipMask,
        PushLayer,
        PopLayer,
        CompositeLayers,
        SaveLayerAsTexture,
        SaveLayerAsMaskImage
    };

    Renderable*        renderable;
    size_t             textureId;
    Ogre::Vector4      scissor;
    Rml::Vector2f      translation;
    Type               type;
    bool               scissorEnabled;
    ClipMaskOperation  clipMaskOp;
    BlendMode          blendMode;
    uint16             stencilValue;
    uint16             transformIndex;
    uint16             srcLayerIndex;
    uint16             destLayerIndex;
    uint16             filterSetIndex;
};

class RenderInterface : public Rml::RenderInterface
{
public:
    RenderInterface();
    ~RenderInterface();

    void AddShaderMaker(std::string_view name, std::unique_ptr<ShaderMaker>&& maker);
    void AddFilterMaker(std::string_view name, std::unique_ptr<FilterMaker>&& maker);

    void OnResourcesLoaded();

    void BeginFrame();
    void EndFrame();

    Rml::CompiledGeometryHandle CompileGeometry(
        Rml::Span<const Rml::Vertex> vertices,
        Rml::Span<const int> indices
    ) final;
    void RenderGeometry(
        Rml::CompiledGeometryHandle geometry,
        Rml::Vector2f translation,
        Rml::TextureHandle texture
    ) final;
    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) final;

    Rml::TextureHandle LoadTexture(
        Rml::Vector2i& texture_dimensions,
        const Rml::String& source
    ) final;
    Rml::TextureHandle GenerateTexture(
        Rml::Span<const Rml::byte> source,
        Rml::Vector2i source_dimensions
    ) final;
    void ReleaseTexture(Rml::TextureHandle texture) final;

    void EnableScissorRegion(bool enable) final;
    void SetScissorRegion(Rml::Rectanglei region) final;

    void SetTransform(const Rml::Matrix4f* transform) final;

    void EnableClipMask(bool enable) final;
    void RenderToClipMask(
        Rml::ClipMaskOperation operation,
        Rml::CompiledGeometryHandle geometry,
        Rml::Vector2f translation
    ) final;

    Rml::LayerHandle PushLayer() final;
    void CompositeLayers(
        Rml::LayerHandle source,
        Rml::LayerHandle destination,
        Rml::BlendMode blend_mode,
        Rml::Span<const Rml::CompiledFilterHandle> filters
    ) final;
    void PopLayer() final;

    Rml::TextureHandle SaveLayerAsTexture() final;
    Rml::CompiledFilterHandle SaveLayerAsMaskImage() final;

    Rml::CompiledFilterHandle CompileFilter(
        const Rml::String& name,
        const Rml::Dictionary& parameters
    ) final;
    void ReleaseFilter(Rml::CompiledFilterHandle filter) final;

    Rml::CompiledShaderHandle CompileShader(
        const Rml::String& name,
        const Rml::Dictionary& parameters
    ) final;
    void RenderShader(
        Rml::CompiledShaderHandle shader,
        Rml::CompiledGeometryHandle geometry,
        Rml::Vector2f translation,
        Rml::TextureHandle texture
    ) final;
    void ReleaseShader(Rml::CompiledShaderHandle shader) final;

public:
    void setSceneManager(Ogre::SceneManager* sceneManager);
    void drawIntoCompositor(Ogre::RenderPassDescriptor* renderPassDesc, Ogre::TextureGpu* anyTargetTexture, Ogre::Camera const* currentCamera, Ogre::RenderTargetViewDef const* rtv);

    void addDrawCommand(DrawCommand const& command);
    void injectNewRenderable(DrawCommand& command, Ogre::MaterialPtr const& material = nullptr);
    DrawCommand* getLastDrawCommand();

    void releaseTexture(Ogre::TextureGpu* texture);

private:
    struct RenderContext
    {
        Ogre::RenderPassDescriptor* passDesc;
        Ogre::TextureGpu*           rtt;
        size_t                      textureId;
    };

    Vector<DrawCommand>  m_drawCommands;
    Ogre::Vector4        m_scissorRef        = { 0.0f, 0.0f, 1.0f, 1.0f };
    bool                 m_scissorEnabled    = false;
    bool                 m_clipMaskEnabled   = false;
    ClipMaskOperation    m_clipMaskOpRef     = ClipMaskOperation::None;
    uint16               m_stencilBaseValue  = 0;
    uint16               m_stencilRefValue   = 0;
    uint16               m_transformRefIndex = UINT16_MAX;
    int16                m_layerIndexRef     = -1;
    int16                m_layerIndexMax     = -1;

    Ogre::MaterialPtr m_baseMaterial;
    Ogre::MaterialPtr m_blankMaterial;
    Ogre::MaterialPtr m_maskMaterial;

    size_t                          m_textureIdCounter = 0;
    UMap<size_t, Ogre::TextureGpu*> m_textures;

    Vector<Ogre::Matrix4>   m_transforms;
    Vector<RenderContext>   m_renderStack;
    Vector<Vector<size_t>>  m_filterSets;

    UMap<size_t, UPtr<ShaderMaker>> m_shaderMakers;
    UMap<size_t, Ogre::MaterialPtr> m_shaderMaterials;
    size_t                          m_shaderFilterIdCounter = 0;

    UMap<size_t, UPtr<FilterMaker>> m_filterMakers;
    UMap<size_t, UPtr<Filter>>      m_filters;

    Vector<Renderable*> m_garbageRenderables;
    Vector<size_t>      m_garbageTextureIds;
    FlatSet<size_t>     m_garbageFilterIds;

    Ogre::IndirectBufferPacked* m_indirectBuffer{};
    Ogre::CommandBuffer*        m_commandBuffer{};

    Ogre::HlmsSamplerblock m_samplerblock;
    Ogre::HlmsMacroblock   m_macroblock;
    Ogre::HlmsBlendblock   m_blendblock;

    Ogre::MovableObject*  m_dummyMovableObject{};
    Ogre::SceneManager*   m_sceneManager{};
    detail::MemoryManager m_memoryManager;

    void createBlankMaterial();
    void createBaseMaterial();
    void createMaskMaterial();

    Ogre::TextureGpu* acquireLayerTexture(size_t textureId, float vpWidth, float vpHeight, Ogre::TextureGpu& mainRTT);

    Ogre::Matrix4 getProjectionMatrix( Ogre::RenderSystem* rs, const bool bRequiresTextureFlipping,
                                    const Ogre::Camera* currentCamera, float vpWidth, float vpHeight ) const;
};

}  // namespace OgreRmlUi
