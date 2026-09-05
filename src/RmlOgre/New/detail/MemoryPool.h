#pragma once

#include "../PoolObject.h"
#include <mimalloc-3.5/mimalloc.h>

namespace RmlOgre::detail
{
    
class MemoryPool
{
public:
    virtual ~MemoryPool() = default;
    virtual void* acquire() = 0;
    virtual void release(void* obj) = 0;
    virtual void resize(size_t size) = 0;
    virtual void freeUnused() = 0;
    virtual void setExpansionSize(size_t size) = 0;
    virtual size_t getSize() const = 0;
    virtual size_t getFreeSize() const = 0;
    virtual mi_heap_t* getHeap() const = 0;

protected:
    MemoryPool() = default;
};

template <typename T> requires std::derived_from<T, PoolObject>
class ObjectPool : public MemoryPool
{
public:
    ObjectPool(size_t size, size_t expansionSize) :
        m_size         (size),
        m_expansionSize(expansionSize),
        m_heap         (mi_heap_new())
    {
        expand(false);
    }

    ~ObjectPool()
    {
        assert(m_freeList.size() == m_size && "You must destroy all objects before closing the pool");

        mi_heap_destroy(m_heap);
    }

    void* acquire() final
    {
        return acquireT();
    }

    void release(void* obj) final
    {
        releaseT(static_cast<T*>(obj));
    }

    void resize(size_t size) final
    {
        if (size <= m_size)
            return;

        m_freeList.reserve(size);
        for (size_t i = 0; i < size - m_size; i++)
        {
            T* obj = static_cast<T*>(mi_heap_malloc(m_heap, sizeof(T)));
            m_freeList.push_back(obj);
        }
        m_size = size;
    }

    void freeUnused() final
    {
        for (T* obj : m_freeList)
        {
            mi_free(obj);
        }
        m_size -= m_freeList.size();
        m_freeList.clear();
        mi_heap_collect(m_heap, true);
    }

    void setExpansionSize(size_t size) final
    {
        m_expansionSize = size;
    }

    size_t getSize() const final
    {
        return m_size;
    }

    size_t getFreeSize() const final
    {
        return m_freeList.size();
    }

    mi_heap_t* getHeap() const final
    {
        return m_heap;
    }

private:
    size_t     m_size{};
    size_t     m_expansionSize{};
    mi_heap_t* m_heap{};
    Vector<T*> m_freeList;

    T* acquireT() 
    {
        if (m_freeList.empty())
            expand();

        T* obj = m_freeList.back(); m_freeList.pop_back();
        return obj;
    }
    
    void releaseT(T* obj)
    {
        if (!obj)
            return;

        obj->~T();
        m_freeList.push_back(obj);
    }

    void expand(bool updateSize = true)
    {
        size_t alloc_count = updateSize ? m_expansionSize : m_size;
        m_freeList.reserve(m_freeList.size() + alloc_count);
    
        for (size_t i = 0; i < alloc_count; i++)
        {
            T* obj = static_cast<T*>(mi_heap_malloc(m_heap, sizeof(T)));
            m_freeList.push_back(obj);
        }
    
        if (updateSize)
            m_size += m_expansionSize;
    }
};

} // namespace RmlOgre
