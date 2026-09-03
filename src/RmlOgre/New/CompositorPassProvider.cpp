#include "CompositorPassProvider.h"

#include "CompositorPass.h"
#include "CompositorPassDef.h"
#include "OgreScriptTranslator.h"

using namespace RmlOgre;

CompositorPassProvider::CompositorPassProvider( Manager *Manager ) :
    mManager( Manager )
{
}
//-------------------------------------------------------------------------
Ogre::CompositorPassDef *CompositorPassProvider::addPassDef( Ogre::CompositorPassType passType,
                                                            Ogre::IdString customId,
                                                            Ogre::CompositorTargetDef *parentTargetDef,
                                                            Ogre::CompositorNodeDef *parentNodeDef )
{
    if( customId == "rmlui" )
    {
        return OGRE_NEW CompositorPassDef( parentTargetDef );
    }

    return 0;
}
//-------------------------------------------------------------------------
Ogre::CompositorPass *CompositorPassProvider::addPass( const Ogre::CompositorPassDef *definition,
                                                        Ogre::Camera* defaultCamera,
                                                        Ogre::CompositorNode *parentNode,
                                                        const Ogre::RenderTargetViewDef *rtvDef,
                                                        Ogre::SceneManager *sceneManager )
{
    // Not created by us.
    if( definition->getCustomId() != Ogre::IdString( "rmlui" ).getU32Value() )
        return 0;

    OGRE_ASSERT_HIGH( dynamic_cast<const CompositorPassDef *>( definition ) );
    const CompositorPassDef *imguiDef =
        static_cast<const CompositorPassDef *>( definition );
    return OGRE_NEW CompositorPass( imguiDef, defaultCamera, sceneManager, rtvDef, parentNode,
                                            mManager );
}
//-------------------------------------------------------------------------
static bool ScriptTranslatorGetBoolean( const Ogre::AbstractNodePtr &node, bool *result )
{
    if( node->type != Ogre::ANT_ATOM )
        return false;
    const Ogre::AtomAbstractNode *atom = (const Ogre::AtomAbstractNode *)node.get();
    if( atom->id == 1 || atom->id == 2 )
    {
        *result = atom->id == 1 ? true : false;
        return true;
    }
    return false;
}
//-------------------------------------------------------------------------
void CompositorPassProvider::translateCustomPass( Ogre::ScriptCompiler *compiler,
                                                        const Ogre::AbstractNodePtr &node,
                                                        Ogre::IdString customId,
                                                        Ogre::CompositorPassDef *customPassDef )
{
    //if( customId != "rmlui" )
        return;  // Custom pass not created by us

    CompositorPassDef *imguiDef = static_cast<CompositorPassDef *>( customPassDef );

    Ogre::ObjectAbstractNode *obj = reinterpret_cast<Ogre::ObjectAbstractNode *>( node.get() );

    obj->context = Ogre::Any( static_cast<CompositorPassDef *>( imguiDef ) );

    Ogre::AbstractNodeList::const_iterator itor = obj->children.begin();
    Ogre::AbstractNodeList::const_iterator endt = obj->children.end();

    while( itor != endt )
    {
        if( ( *itor )->type == Ogre::ANT_OBJECT )
        {
            Ogre::ObjectAbstractNode *childObj = reinterpret_cast<Ogre::ObjectAbstractNode *>( itor->get() );

            if( childObj->id == Ogre::ID_LOAD )
            {
                Ogre::CompositorLoadActionTranslator compositorLoadActionTranslator;
                compositorLoadActionTranslator.translate( compiler, *itor );
            }
            else if( childObj->id == Ogre::ID_STORE )
            {
                Ogre::CompositorStoreActionTranslator compositorStoreActionTranslator;
                compositorStoreActionTranslator.translate( compiler, *itor );
            }
        }
        else if( ( *itor )->type == Ogre::ANT_PROPERTY )
        {
            const Ogre::PropertyAbstractNode *prop =
                reinterpret_cast<const Ogre::PropertyAbstractNode *>( itor->get() );
            if( prop->name == "sets_resolution" )
            {
                if( prop->values.size() != 1u ||
                    !ScriptTranslatorGetBoolean( prop->values.front(), &imguiDef->mSetsResolution ) )
                {
                    compiler->addError( Ogre::ScriptCompiler::CE_STRINGEXPECTED, obj->file, obj->line,
                                        "sets_resolution expects a boolean value" );
                }
            }
        }
        ++itor;
    }
}
