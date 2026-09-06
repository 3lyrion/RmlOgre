// Credits: 3lyrion [OgreRmlUi]

#pragma once

#include <OgreRmlUi/detail/MemoryPool.h>

namespace OgreRmlUi::detail
{

class MemoryManager
{
public:
    MemoryManager() = default;

    MemoryManager(MemoryManager const&)              = delete;
    MemoryManager& operator = (MemoryManager const&) = delete;

    template <typename T, size_t size = 16, size_t expansionSize = 16>
    void registerPool()
    {
        static_assert(expansionSize > 0, "Pool's expansion size cannot be 0");

        constexpr auto& tid = typeid(T);
        auto& pool = m_pools[tid];
        if (pool)
        {
            throw std::runtime_error("Pool for '" + String(tid.name()) + "' is ALREADY registered");
        }
        pool = std::make_unique<detail::ObjectPool<T>>(size, expansionSize);
    }

    template <typename T>
    void unregisterPool()
    {
        constexpr auto& tid = typeid(T);
        m_pools.erase(tid);
    }

    template <typename T>
    void resizePool(size_t size)
    {
        auto& pool = getPool<T>();
        pool.resize(size);
    }

    template <typename T>
    void setPoolExpansionSize(size_t size)
    {
        auto& pool = getPool<T>();
        pool.setExpansionSize(size);
    }

    template <typename T>
    void freeUnusedPoolMemory()
    {
        auto& pool = getPool<T>();
        pool.freeUnused();
    }

    template <typename T>
    bool isPoolRegistered() const
    {
        constexpr auto& tid = typeid(T);
        return m_pools.contains(tid);
    }

    template <typename T>
    size_t getPoolSize() const
    {
        auto& pool = getPool<T>();
        return pool.getSize();
    }

    template <typename T>
    size_t getPoolFreeSize() const
    {
        auto& pool = getPool<T>();
        return pool.getFreeSize();
    }

    template <typename T>
    T& create(auto&&... args)
    {
        auto& pool = getPool<T>();
        T* obj = static_cast<T*>(pool.acquire());
        ::new(obj) T(std::forward<decltype(args)>(args)...);
        return *obj;
    }

    void destroy(void* object, TypeInfo const& typeInfo)
    {
        if (!object)
            return;

        auto& pool = getPool(typeInfo);
        pool.release(object);
    }

    template <typename T> requires std::is_reference_v<T&&>
    void destroy(T& object)
    {
        auto& pool = getPool<T>();
        pool.release(&object);
    }

private:
    using PoolPtr = UPtr<detail::MemoryPool>;

    UMap<TypeIndex, PoolPtr> m_pools;

    template <typename T>
    detail::MemoryPool& getPool() const
    {
        constexpr auto& tid = typeid(T);
        auto pool = find(m_pools, tid);
        if (!pool)
        {
            throw std::runtime_error("Pool for '" + String(tid.name()) + "' is not registered");
        }
        return **pool;
    }

    detail::MemoryPool& getPool(TypeInfo const& tid) const
    {
        auto pool = find(m_pools, tid);
        if (!pool)
        {
            throw std::runtime_error("Pool for '" + String(tid.name()) + "' is not registered");
        }
        return **pool;
    }
};

} // namespace mr
