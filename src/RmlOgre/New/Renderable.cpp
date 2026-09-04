#include "Renderable.h"

#include "Vao/OgreVaoManager.h"
#include "Vao/OgreVertexArrayObject.h"

using namespace RmlOgre;

namespace
{
    Ogre::VertexElement2Vec VertexFormat;
    uint32_t                VertexSize = 0;

    struct GUIVertex
    {
        Ogre::Vector2     position;
        Ogre::ColourValue color;
        Ogre::Vector2     uv;

        GUIVertex(Rml::Vertex const& v) :
            position(v.position.x, v.position.y),
            color   {
                float(v.colour.red)   / 255.0f,
                float(v.colour.green) / 255.0f,
                float(v.colour.blue)  / 255.0f,
                float(v.colour.alpha) / 255.0f},
            uv      (v.tex_coord.x, v.tex_coord.y)
        { }

	    template <class Iterator>
	    void write(Iterator& iterator) const
	    {
		    *(iterator++) = position.x;
		    *(iterator++) = position.y;

		    *(iterator++) = color.r;
		    *(iterator++) = color.g;
		    *(iterator++) = color.b;
		    *(iterator++) = color.a;

		    *(iterator++) = uv.x;
		    *(iterator++) = uv.y;
	    }
    };
}

Renderable::Renderable()
{
    // use identity projection and view matrices
    mUseIdentityProjection = true;
    mUseIdentityView = true;

    // By default we want Renderables to still work in wireframe mode
    mPolygonModeOverrideable = false;
}
//-----------------------------------------------------------------------------
Renderable::~Renderable()
{
}
//-----------------------------------------------------------------------------

void Renderable::setOwningCommand(uint16_t index, size_t id)
{
    m_owningCommandIndex = index;
    m_owningCommandId    = id;
}

std::pair<uint16_t, size_t> Renderable::getOwningCommand() const
{
    return { m_owningCommandIndex, m_owningCommandId };
}

size_t Renderable::getVertexCount() const
{
    auto& vaos = mVaoPerLod[Ogre::VpNormal];
    if (vaos.empty())
        return 0;

    return vaos[0]->getBaseVertexBuffer()->getNumElements();
}
//-----------------------------------------------------------------------------
size_t Renderable::getIndexCount() const
{
    auto& vaos = mVaoPerLod[Ogre::VpNormal];
    if (vaos.empty())
        return 0;

    return vaos[0]->getIndexBuffer()->getNumElements();
}
//-----------------------------------------------------------------------------
void Renderable::recreateBuffers(Ogre::VaoManager *vaoManager, Ogre::VertexBufferPacked *newVertexBuffer,
                                       Ogre::IndexBufferPacked *newIndexBuffer)
{
    auto& vaos = mVaoPerLod[Ogre::VpNormal];
    for(auto* vao : vaos)
    {
        auto& vertexBuffers = vao->getVertexBuffers();
        for (auto* vertexBuffer : vertexBuffers)
        {
            if (newVertexBuffer)
            {
                if (vertexBuffer->getMappingState() != Ogre::MS_UNMAPPED)
                    vertexBuffer->unmap(Ogre::UO_UNMAP_ALL);

                vaoManager->destroyVertexBuffer(vertexBuffer);
            }
            else
                newVertexBuffer = vertexBuffer;
        }

        if (newIndexBuffer)
        {
            if (auto* indexBuffer = vao->getIndexBuffer())
            {
                if (indexBuffer->getMappingState() != Ogre::MS_UNMAPPED)
                    indexBuffer->unmap(Ogre::UO_UNMAP_ALL);

                vaoManager->destroyIndexBuffer(indexBuffer);
            }
        }
        else
            newIndexBuffer = vao->getIndexBuffer();

        vaoManager->destroyVertexArrayObject(vao);
    }

    auto vao = vaoManager->createVertexArrayObject({ newVertexBuffer }, newIndexBuffer, Ogre::OT_TRIANGLE_LIST);
    vaos.clear();
    vaos.push_back(vao);
}
//-----------------------------------------------------------------------------
void Renderable::destroyBuffers(Ogre::VaoManager *vaoManager)
{
    auto& vaos = mVaoPerLod[Ogre::VpNormal];
    for(auto* vao : vaos)
    {
        auto& vertexBuffers = vao->getVertexBuffers();
        for (auto* vertexBuffer : vertexBuffers)
        {
            if (vertexBuffer->getMappingState() != Ogre::MS_UNMAPPED)
                vertexBuffer->unmap(Ogre::UO_UNMAP_ALL);

            vaoManager->destroyVertexBuffer(vertexBuffer);
        }
        if (auto* indexBuffer = vao->getIndexBuffer())
        {
            if (indexBuffer->getMappingState() != Ogre::MS_UNMAPPED)
                indexBuffer->unmap(Ogre::UO_UNMAP_ALL);

            vaoManager->destroyIndexBuffer(indexBuffer);
        }
        vaoManager->destroyVertexArrayObject(vao);
    }
    vaos.clear();
}
//-----------------------------------------------------------------------------
void Renderable::updateVertexData(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices, Ogre::VaoManager *vaoManager)
{
    Ogre::VertexBufferPacked* vertexBuffer = nullptr;
    Ogre::IndexBufferPacked*  indexBuffer  = nullptr;
    if (vertices.size() > getVertexCount())
    {
        if (VertexFormat.empty())
        {
            VertexFormat.push_back(Ogre::VertexElement2(Ogre::VET_FLOAT2, Ogre::VES_POSITION));
            VertexFormat.push_back(Ogre::VertexElement2(Ogre::VET_FLOAT4, Ogre::VES_DIFFUSE));
            VertexFormat.push_back(Ogre::VertexElement2(Ogre::VET_FLOAT2, Ogre::VES_TEXTURE_COORDINATES));
	        VertexSize = vaoManager->calculateVertexSize(VertexFormat);
        }

	    auto* ogreVertices = reinterpret_cast<float*>(OGRE_MALLOC_SIMD(
		    vertices.size() * VertexSize,
		    Ogre::MEMCATEGORY_GEOMETRY));
	    auto verticesIter = ogreVertices;
	    for (auto& v : vertices)
		    GUIVertex{v}.write(verticesIter);

        vertexBuffer = vaoManager->createVertexBuffer(VertexFormat, vertices.size(), Ogre::BT_DEFAULT, ogreVertices, false);
        OGRE_FREE_SIMD(ogreVertices, Ogre::MEMCATEGORY_GEOMETRY);
    }
    if (indices.size() > getIndexCount())
    {
	    auto* ogreIndices = reinterpret_cast<Ogre::uint16*>(OGRE_MALLOC_SIMD(
		    indices.size() * sizeof(Ogre::uint16),
		    Ogre::MEMCATEGORY_GEOMETRY));
	    for (std::size_t i = 0; i < indices.size(); ++i)
		    ogreIndices[i] = (uint16_t)indices[i];

        indexBuffer = vaoManager->createIndexBuffer(Ogre::IndexBufferPacked::IT_16BIT, indices.size(), Ogre::BT_IMMUTABLE, ogreIndices, false);
        OGRE_FREE_SIMD(ogreIndices, Ogre::MEMCATEGORY_GEOMETRY);
    }

    if (vertexBuffer || indexBuffer)
        recreateBuffers(vaoManager, vertexBuffer, indexBuffer);
}
//-----------------------------------------------------------------------------
void Renderable::getWorldTransforms(Ogre::Matrix4 *xform) const
{
    OGRE_EXCEPT(Ogre::Exception::ERR_NOT_IMPLEMENTED,
                 "Renderable do not implement getWorldTransforms."
                 " You've put a v2 object in "
                 "the wrong RenderQueue ID (which is set to be compatible with "
                 "v1::Entity). Do not mix v2 and v1 objects",
                 "Renderable::getWorldTransforms");
}
//-----------------------------------------------------------------------------
void Renderable::getRenderOperation(Ogre::v1::RenderOperation &op, bool casterPass)
{
    OGRE_EXCEPT(Ogre::Exception::ERR_NOT_IMPLEMENTED,
                 "Renderable do not implement getRenderOperation."
                 " You've put a v2 object in "
                 "the wrong RenderQueue ID (which is set to be compatible with "
                 "v1::Entity). Do not mix v2 and v1 objects",
                 "Renderable::getRenderOperation");
}
//-----------------------------------------------------------------------------
const Ogre::LightList &Renderable::getLights(void) const
{
    static const Ogre::LightList l;
    return l;
}
