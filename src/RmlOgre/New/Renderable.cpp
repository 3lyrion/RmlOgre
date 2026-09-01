#include "Renderable.h"

#include "Vao/OgreVaoManager.h"
#include "Vao/OgreVertexArrayObject.h"

using namespace RmlOgre;

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
size_t Renderable::getVertexCount() const
{
    if( mVaoPerLod[0].empty() )
        return 0u;
    return mVaoPerLod[0].front()->getBaseVertexBuffer()->getNumElements();
}
//-----------------------------------------------------------------------------
size_t Renderable::getIndexCount() const
{
    if( mVaoPerLod[0].empty() )
        return 0u;
    return mVaoPerLod[0].front()->getIndexBuffer()->getNumElements();
}
//-----------------------------------------------------------------------------
void Renderable::recreateBuffers(Ogre::VaoManager *vaoManager, Ogre::VertexBufferPacked *newVertexBuffer,
                                       Ogre::IndexBufferPacked *newIndexBuffer )
{
    for(auto *vao : mVaoPerLod[0] )
    {
        auto &vertexBuffers = vao->getVertexBuffers();
        for( auto *vertexBuffer : vertexBuffers )
        {
            if( newVertexBuffer )
            {
                if( vertexBuffer->getMappingState() != Ogre::MS_UNMAPPED )
                    vertexBuffer->unmap( Ogre::UO_UNMAP_ALL );
                vaoManager->destroyVertexBuffer( vertexBuffer );
            }
            else
            {
                newVertexBuffer = vertexBuffer;
            }
        }

        if( newIndexBuffer )
        {
            auto *indexBuffer = vao->getIndexBuffer();
            if( indexBuffer )
            {
                if( indexBuffer->getMappingState() != Ogre::MS_UNMAPPED )
                    indexBuffer->unmap( Ogre::UO_UNMAP_ALL );
                vaoManager->destroyIndexBuffer( indexBuffer );
            }
        }
        else
        {
            newIndexBuffer = vao->getIndexBuffer();
        }
        vaoManager->destroyVertexArrayObject( vao );
    }

    mVaoPerLod[0].clear();
    mVaoPerLod[0].push_back(
        vaoManager->createVertexArrayObject( { newVertexBuffer }, newIndexBuffer, Ogre::OT_TRIANGLE_LIST ) );
}
//-----------------------------------------------------------------------------
void Renderable::destroyBuffers( Ogre::VaoManager *vaoManager )
{
    for( auto *vao : mVaoPerLod[0] )
    {
        const auto &vertexBuffers = vao->getVertexBuffers();
        for( auto *vertexBuffer : vertexBuffers )
        {
            if( vertexBuffer->getMappingState() != Ogre::MS_UNMAPPED )
                vertexBuffer->unmap( Ogre::UO_UNMAP_ALL );
            vaoManager->destroyVertexBuffer( vertexBuffer );
        }

        auto *indexBuffer = vao->getIndexBuffer();
        if( indexBuffer )
        {
            if( indexBuffer->getMappingState() != Ogre::MS_UNMAPPED )
                indexBuffer->unmap( Ogre::UO_UNMAP_ALL );
            vaoManager->destroyIndexBuffer( indexBuffer );
        }
        vaoManager->destroyVertexArrayObject( vao );
    }
}
//-----------------------------------------------------------------------------
void Renderable::updateVertexData( const Rml::Vertex *vtxBuf, const int *idxBuf,
                                        unsigned int vtxCount, unsigned int idxCount,
                                        Ogre::VaoManager *vaoManager )
{
    Ogre::VertexBufferPacked *vertexBuffer = 0;
    Ogre::IndexBufferPacked *indexBuffer = 0;
    if( vtxCount > getVertexCount() )
    {
        Ogre::VertexElement2Vec vertexElements;
        vertexElements.push_back( Ogre::VertexElement2( Ogre::VET_FLOAT2, Ogre::VES_POSITION ) );
        vertexElements.push_back( Ogre::VertexElement2( Ogre::VET_COLOUR, Ogre::VES_DIFFUSE ) );
        vertexElements.push_back( Ogre::VertexElement2( Ogre::VET_FLOAT2, Ogre::VES_TEXTURE_COORDINATES ) );

        vertexBuffer = vaoManager->createVertexBuffer( vertexElements, size_t( vtxCount ),
                                                       Ogre::BT_DYNAMIC_PERSISTENT, nullptr, false );
    }
    if( idxCount > getIndexCount() )
    {
        indexBuffer = vaoManager->createIndexBuffer( Ogre::IndexBufferPacked::IT_16BIT, size_t( idxCount ),
                                                     Ogre::BT_DYNAMIC_PERSISTENT, nullptr, false );
    }

    if( vertexBuffer || indexBuffer )
        recreateBuffers( vaoManager, vertexBuffer, indexBuffer );

    auto *vao = mVaoPerLod[0][0];
    vao->setPrimitiveRange( 0u, idxCount );

    void *vtxDst = vao->getBaseVertexBuffer()->map( 0u, vtxCount );
    void *idxDst = vao->getIndexBuffer()->map( 0u, idxCount );

    // Copy all vertices
    memcpy( vtxDst, vtxBuf, vtxCount * sizeof( Rml::Vertex ) );
    memcpy( idxDst, idxBuf, idxCount * sizeof( int ) );

    vao->getBaseVertexBuffer()->unmap( Ogre::UO_KEEP_PERSISTENT, 0u, vtxCount );
    vao->getIndexBuffer()->unmap( Ogre::UO_KEEP_PERSISTENT, 0u, idxCount );
}
//-----------------------------------------------------------------------------
void Renderable::getWorldTransforms( Ogre::Matrix4 *xform ) const
{
    OGRE_EXCEPT( Ogre::Exception::ERR_NOT_IMPLEMENTED,
                 "Renderable do not implement getWorldTransforms."
                 " You've put a v2 object in "
                 "the wrong RenderQueue ID (which is set to be compatible with "
                 "v1::Entity). Do not mix v2 and v1 objects",
                 "Renderable::getWorldTransforms" );
}
//-----------------------------------------------------------------------------
void Renderable::getRenderOperation(Ogre::v1::RenderOperation &op, bool casterPass )
{
    OGRE_EXCEPT( Ogre::Exception::ERR_NOT_IMPLEMENTED,
                 "Renderable do not implement getRenderOperation."
                 " You've put a v2 object in "
                 "the wrong RenderQueue ID (which is set to be compatible with "
                 "v1::Entity). Do not mix v2 and v1 objects",
                 "Renderable::getRenderOperation" );
}
//-----------------------------------------------------------------------------
const Ogre::LightList &Renderable::getLights( void ) const
{
    static const Ogre::LightList l;
    return l;
}
