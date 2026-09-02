#pragma once

#include <OgreRenderOperation.h>
#include <OgreRenderable.h>

#include <RmlUi/Core.h>

#include "OgreHeaderPrefix.h"

namespace RmlOgre
{

class Renderable final : public Ogre::Renderable
{
    void recreateBuffers(Ogre::VaoManager *vaoManager, Ogre::VertexBufferPacked *vertexBuffer, Ogre::IndexBufferPacked *indexBuffer );

    size_t getVertexCount() const;
    size_t getIndexCount() const;

public:
    Renderable();
    ~Renderable() override;

    void destroyBuffers( Ogre::VaoManager *vaoManager );

    // builds the vertex buffer
    void updateVertexData(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices, Ogre::VaoManager *vaoManager );

    // Overrides from Renderable
    void getWorldTransforms( Ogre::Matrix4 *xform ) const override;
    void getRenderOperation( Ogre::v1::RenderOperation &op, bool casterPass ) override;

    const Ogre::LightList &getLights( void ) const override;
};

} // namespace RmlOgre
