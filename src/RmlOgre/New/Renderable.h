#pragma once

#include <OgreRenderOperation.h>
#include <OgreRenderable.h>

#include <RmlUi/Core.h>

#include "OgreHeaderPrefix.h"

namespace RmlOgre
{

class Renderable final : public Ogre::Renderable
{
    uint16_t m_owningCommandIndex = 0;
    size_t   m_owningCommandId    = 0;

    void recreateBuffers(Ogre::VaoManager *vaoManager, Ogre::VertexBufferPacked *vertexBuffer, Ogre::IndexBufferPacked *indexBuffer );

    size_t getVertexCount() const;
    size_t getIndexCount() const;

public:
    Renderable();
    ~Renderable() final;

    void setOwningCommand(uint16_t index, size_t id);
    std::pair<uint16_t, size_t> getOwningCommand() const;

    void destroyBuffers( Ogre::VaoManager *vaoManager );

    // builds the vertex buffer
    void updateVertexData(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices, Ogre::VaoManager *vaoManager );

    // Overrides from Renderable
    void getWorldTransforms( Ogre::Matrix4 *xform ) const final;
    void getRenderOperation( Ogre::v1::RenderOperation &op, bool casterPass ) final;

    const Ogre::LightList &getLights( void ) const final;
};

} // namespace RmlOgre
