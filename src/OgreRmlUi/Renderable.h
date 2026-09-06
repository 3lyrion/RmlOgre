// Credits: 3lyrion [OgreRmlUi], nimble [OgreRmlUi], the ogre-next team [ImGui]

#pragma once

#include <OgreRmlUi/detail/PoolObject.h>

#include <OgreRenderOperation.h>
#include <OgreRenderable.h>

namespace OgreRmlUi
{

class Renderable : public Ogre::Renderable, public detail::PoolObject
{
public:
    Renderable();

    void destroyBuffers( Ogre::VaoManager *vaoManager );

    void shareSameVAO(Renderable const& other);

    // builds the vertex buffer
    void updateVertexData(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices, Ogre::VaoManager *vaoManager );

    // Overrides from Renderable
    void getWorldTransforms( Ogre::Matrix4 *xform ) const final;
    void getRenderOperation( Ogre::v1::RenderOperation &op, bool casterPass ) final;

    const Ogre::LightList &getLights( void ) const final;

private:
    bool m_hasDependency = false;

    void recreateBuffers(Ogre::VaoManager *vaoManager, Ogre::VertexBufferPacked *vertexBuffer, Ogre::IndexBufferPacked *indexBuffer );

    size_t getVertexCount() const;
    size_t getIndexCount() const;
};

} // namespace OgreRmlUi
