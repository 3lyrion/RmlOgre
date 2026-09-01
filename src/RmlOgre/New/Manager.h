#pragma once

#include <RmlUi/Core.h>
#include "Prerequisites.h"

#include <map>

namespace RmlOgre
{

class Renderable;

class Manager : public Rml::RenderInterface
{
    typedef std::vector<Renderable*>                    RenderableVec;
    typedef std::map<Ogre::TextureGpu* , RenderableVec> RenderableMap;

    struct Command
    {
        //CmdType                     type;
        //bool                        stateEnabled    = false;
        //uint32_t                    stencilRefValue = 0;
        //Ogre::HlmsMacroblock const* macroblock      = nullptr;
        //Ogre::VertexArrayObject*    vao             = nullptr;
        //Ogre::HlmsDatablock*        datablock       = nullptr;
        //Ogre::Matrix4               transform       = Ogre::Matrix4::IDENTITY;

        //uint32_t          vertexOffset;
        //uint32_t          indexOffset;
        //uint32_t          indexCount;
        //Ogre::Vector4     scissor;
        //Ogre::TextureGpu* texture;

        Ogre::TextureGpu* texture;
        Renderable* renderable;
        Ogre::Vector4 scissor;
        bool scissorEnabled;
        Ogre::Matrix4 transform;
    };

    RenderableMap mAvailableRenderables;
    RenderableVec mScheduledRenderables;

    std::vector<Command> mDrawCmds;
    //std::vector<Rml::Vertex> mTempVertices;
    //std::vector<int> mTempIndices;
    //uint32_t mCurrentVertexCount = 0;
    //uint32_t mCurrentIndexCount = 0;
    Ogre::Vector4 mCurrentScissor;
    bool mScissorEnabled = false;
    Ogre::Matrix4 mCurrentTransform;

    Ogre::IndirectBufferPacked* mIndirectBuffer;
    Ogre::CommandBuffer*        mCommandBuffer;

    Ogre::MovableObject *mDummyMovableObject;

    /// Ensures all shaders are created.
    void createPrograms();

    /// Creates a Material for the given texture. Currently, Texture MUST not be nullptr.
    /// Assumes shaders exist (see createPrograms()).
    Ogre::MaterialPtr createMaterialFor( Ogre::TextureGpu* texture );

    /// Recycles or creates a new Renderable that can display the given texture.
    Ogre::Renderable *getAvailableRenderable( Ogre::TextureGpu* texture );

    /// Frees all resources.
    void destroyAllResources();

    Ogre::Matrix4 getProjectionMatrix( Ogre::RenderSystem* rs, const bool bRequiresTextureFlipping,
                                    const Ogre::Camera* currentCamera, float vpWidth, float vpHeight ) const;

public:
    Manager();
    ~Manager();

    //void prepareForRender( Ogre::SceneManager *sceneManager );

    void drawIntoCompositor( Ogre::RenderPassDescriptor* renderPassDesc, Ogre::TextureGpu* anyTargetTexture,
                                Ogre::SceneManager *sceneManager, const Ogre::Camera* currentCamera );

    void BeginFrame();
    void EndFrame();

	Rml::CompiledGeometryHandle CompileGeometry(
		Rml::Span<const Rml::Vertex> vertices,
		Rml::Span<const int> indices
	) override;
	void RenderGeometry(
		Rml::CompiledGeometryHandle geometry,
		Rml::Vector2f translation,
		Rml::TextureHandle texture
	) override;
	void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

	Rml::TextureHandle LoadTexture(
		Rml::Vector2i& texture_dimensions,
		const Rml::String& source
	) override;
	Rml::TextureHandle GenerateTexture(
		Rml::Span<const Rml::byte> source,
		Rml::Vector2i source_dimensions
	) override;
	void ReleaseTexture(Rml::TextureHandle texture) override;

	void EnableScissorRegion(bool enable) override;
	void SetScissorRegion(Rml::Rectanglei region) override;

	void SetTransform(const Rml::Matrix4f* transform) override;

    /*
	void EnableClipMask(bool enable) override;
	void RenderToClipMask(
		Rml::ClipMaskOperation operation,
		Rml::CompiledGeometryHandle geometry,
		Rml::Vector2f translation
	) override;

	Rml::LayerHandle PushLayer() override;
	void CompositeLayers(
		Rml::LayerHandle source,
		Rml::LayerHandle destination,
		Rml::BlendMode blend_mode,
		Rml::Span<const Rml::CompiledFilterHandle> filters
	) override;
	void PopLayer() override;

	Rml::CompiledFilterHandle CompileFilter(
		const Rml::String& name,
		const Rml::Dictionary& parameters
	) override;
	void ReleaseFilter(Rml::CompiledFilterHandle filter) override;

	Rml::TextureHandle SaveLayerAsTexture() override;
	Rml::CompiledFilterHandle SaveLayerAsMaskImage() override;

	Rml::CompiledShaderHandle CompileShader(
		const Rml::String& name,
		const Rml::Dictionary& parameters
	) override;
	void RenderShader(
		Rml::CompiledShaderHandle shader,
		Rml::CompiledGeometryHandle geometry,
		Rml::Vector2f translation,
		Rml::TextureHandle texture
	) override;
	void ReleaseShader(Rml::CompiledShaderHandle shader) override;
    */
};

}  // namespace RmlOgre
