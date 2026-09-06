// Credits: 3lyrion [OgreRmlUi]

#pragma once

#include <OgreRmlUi/detail/Precompiled.h>

namespace OgreRmlUi::detail
{

class PoolObject
{
protected:
    PoolObject() = default;
    ~PoolObject() = default;

    PoolObject(PoolObject const&) = delete;
    PoolObject& operator = (PoolObject const&) = delete;

    PoolObject(PoolObject&&) noexcept = delete;
    PoolObject& operator = (PoolObject&&) noexcept = delete;

    void operator delete  (void*) noexcept = delete;
    void operator delete[](void*) noexcept = delete;

    void* operator new  (size_t) noexcept(false) = delete;
    void* operator new[](size_t) noexcept(false) = delete;
};

}
