#ifndef NIMBLE_RMLOGRE_RENDERINTERFACE_HPP
#define NIMBLE_RMLOGRE_RENDERINTERFACE_HPP

#include "Precompiled.h"

#include "FilterMaker.hpp"
#include "Material.hpp"
#include "ObjectIndex.hpp"
#include "ShaderMaker.hpp"
#include "Workspace.hpp"
#include "filters.hpp"

#include <OgreHlmsDatablock.h>
#include <OgreHlmsSamplerblock.h>

#include <RmlUi/Core/RenderInterface.h>


namespace Ogre {

class HlmsUnlit;
class HlmsUnlitDatablock;
class SceneManager;
class TextureGpu;

}

namespace nimble::RmlOgre {

struct Layer
{
	int connectionId = -1;
	int copyPass = -1;

	Layer take()
	{
		Layer self = *this;
		this->connectionId = -1;
		return self;
	}
	bool isTaken() const
	{
		return this->connectionId == -1;
	}
};

class RenderInterface : public Rml::RenderInterface
{
	Ogre::HlmsUnlit* hlms = nullptr;
	Ogre::HlmsMacroblock macroblock;
	Ogre::HlmsBlendblock blendblock;
	Ogre::HlmsSamplerblock samplerblock;
	ObjectIndex<Material> materials;

	std::unordered_map<Rml::String, std::unique_ptr<FilterMaker>> filterMakers;
	MaskImageFilterMaker maskImageFilterMaker;
	ObjectIndex<std::unique_ptr<Filter>> filters;

	std::unordered_map<Rml::String, std::unique_ptr<ShaderMaker>> shaderMakers;
	ObjectIndex<Material> shaders;

	RenderPassSettings renderPassSettings;
	int connectionId = 0;
	Vector<Layer> layerBuffers;
	int numActiveLayers = 0;
	Passes passes;
    size_t passIndex;

	int datablockId = 0;
	Vector<Rml::CompiledGeometryHandle> releaseGeometries;
	Vector<Rml::TextureHandle> releaseTextures;
	Vector<Ogre::TextureGpu*> releaseRenderTextures;

	Workspace workspace;

	void releaseBufferedGeometries();
	void releaseBufferedTextures();

	template <class TRenderPass>
	TRenderPass& getRenderPass()
	{
		TRenderPass* lastPass = nullptr;
		if(!this->passes.empty())
			lastPass = std::get_if<TRenderPass>(&this->passes.back());
		if(!lastPass || lastPass->settings != this->renderPassSettings)
		{
			TRenderPass newPass;
			newPass.settings = this->renderPassSettings;
			this->passes.push_back(std::move(newPass));
			lastPass = &std::get<TRenderPass>(this->passes.back());
		}

		return *lastPass;
	}

public:
	RenderInterface(
		const Ogre::String& name,
		Ogre::SceneManager* sceneManager,
		Ogre::TextureGpu* output,
		Ogre::TextureGpu* background = nullptr);


	int addConnection() { return this->connectionId++; }
	Layer getLayerBuffer(int index);
	void putLayerBuffer(int index, Layer id);
	Layer acquireLayerBuffer();
	void releaseLayerBuffer(Layer id);

	const RenderPassSettings& currentRenderPassSettings() const { return this->renderPassSettings; }
	void addPass(Pass&& pass);
	void releaseRenderTexture(Ogre::TextureGpu* texture);


	void AddFilterMaker(Rml::String name, std::unique_ptr<FilterMaker> filterMaker);
	void AddShaderMaker(Rml::String name, std::unique_ptr<ShaderMaker> shaderMaker);

	Ogre::TextureGpu* GetOutput() const       { return this->workspace.output(); }
	void SetOutput(Ogre::TextureGpu* texture) { this->workspace.output(texture); }
	Ogre::TextureGpu* GetBackground() const       { return this->workspace.background(); }
	void SetBackground(Ogre::TextureGpu* texture) { this->workspace.background(texture); }


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

	Rml::CompiledFilterHandle CompileFilter(
		const Rml::String& name,
		const Rml::Dictionary& parameters
	) final;
	void ReleaseFilter(Rml::CompiledFilterHandle filter) final;

	Rml::TextureHandle SaveLayerAsTexture() final;
	Rml::CompiledFilterHandle SaveLayerAsMaskImage() final;

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

}

#endif // NIMBLE_RMLOGRE_RENDERINTERFACE_HPP
