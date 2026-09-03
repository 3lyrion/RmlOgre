#pragma once

#include "Shaders.h"

#include <map>

namespace RmlOgre
{

class Renderable;

class Manager : public Rml::RenderInterface
{
    enum class ClipMaskOperation : int8_t
    {
        None = -1,
        Set = Rml::ClipMaskOperation::Set,
        SetInverse = Rml::ClipMaskOperation::SetInverse,
        Intersect = Rml::ClipMaskOperation::Intersect
    };

    enum class CmdType : uint8_t
    {
        Geometry = 0,
        ClipMask
    };

    struct Command
    {
        Renderable*        renderable;
        Ogre::TextureGpu*  texture;
        Ogre::Vector4      scissor;
        CmdType            type;
        bool               scissorEnabled;
        uint16_t           stencilValue = 0;
        ClipMaskOperation  clipMaskOp = ClipMaskOperation::None;
        Ogre::Matrix4      transform = Ogre::Matrix4::IDENTITY;
    };

    struct Shader
    {
        std::unique_ptr<ShaderMaker> maker;
        Ogre::MaterialPtr            material;
    };

    std::vector<Command> mDrawCmds;
    //std::vector<uint32_t> m_garbageDrawCmds;
    Ogre::Vector4 mCurrentScissor = { 0.0f, 0.0f, 1.0f, 1.0f };
    bool mScissorEnabled = false;
    bool mClipMaskEnabled = false;
    uint16_t mStencilBaseValue = 0;
    uint16_t mStencilRefValue = 0;
    ClipMaskOperation mCurrentClipMaskOp = ClipMaskOperation::None;
    Ogre::Matrix4 mCurrentTransform = Ogre::Matrix4::IDENTITY;

    Rml::SmallUnorderedMap<size_t, Shader> m_shaders;

    Ogre::IndirectBufferPacked* mIndirectBuffer;
    Ogre::CommandBuffer*        mCommandBuffer;

    Ogre::HlmsSamplerblock m_samplerblock;
    Ogre::HlmsMacroblock   m_macroblock;
    Ogre::HlmsBlendblock   m_blendblock;
    Ogre::MaterialPtr      m_baseMaterial;
    Ogre::MaterialPtr      m_blankMaterial;
    Ogre::MaterialPtr      m_maskMaterial;
    Ogre::TextureGpu*      m_blankTexture{};

    Ogre::MovableObject *mDummyMovableObject;
    Ogre::SceneManager*  m_sceneManager{};

    /// Ensures all shaders are created.
    void createPrograms();

    /// Frees all resources.
    void destroyAllResources();

    Ogre::Matrix4 getProjectionMatrix( Ogre::RenderSystem* rs, const bool bRequiresTextureFlipping,
                                    const Ogre::Camera* currentCamera, float vpWidth, float vpHeight ) const;

    void createBlankTexture();
    void createBlankMaterial();
    void createBaseMaterial();
    void createMaskMaterial();

public:
    Manager();
    ~Manager();

    //void prepareForRender( Ogre::SceneManager *sceneManager );

    void drawIntoCompositor( Ogre::RenderPassDescriptor* renderPassDesc, Ogre::TextureGpu* anyTargetTexture,
                                Ogre::SceneManager *sceneManager, const Ogre::Camera* currentCamera );

    //Ogre::MaterialPtr CreateBlankMaterial();

    void AddShaderMaker(std::string_view name, std::unique_ptr<ShaderMaker>&& maker);

    void OnResourcesLoaded();

    void SetSceneManager(Ogre::SceneManager& sceneManager);

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
    
};

}  // namespace RmlOgre
