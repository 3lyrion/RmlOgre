#pragma once

#include "OgrePrerequisites.h"

#include <Ogre.h>
#include <RmlUi/Core.h>

namespace RmlOgre
{

class CompositorPassProvider;
class CompositorPassDef;
class Manager;
class Renderable;

template <typename T>
using Hash = robin_hood::hash<T>;

inline constexpr Hash<std::string_view> StringHasher;

}  // namespace RmlOgre
