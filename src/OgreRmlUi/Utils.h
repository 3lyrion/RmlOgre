#pragma once

#include <OgreRmlUi/detail/Precompiled.h>

namespace OgreRmlUi::util
{

inline constexpr float k_rmlColorToOgreMult = 1.0f / 255.0f;

__forceinline Ogre::Vector4 toOgre(Rml::ColourbPremultiplied c)
{
	Ogre::Vector4 result;
	for (int i = 0; i < 4; i++)
    {
		result[i] = k_rmlColorToOgreMult * c[i];
    }
	return result;
}

__forceinline Ogre::Vector2 toOgre(Rml::Vector2f v)
{
	return Ogre::Vector2{ v.x, v.y };
}

}
