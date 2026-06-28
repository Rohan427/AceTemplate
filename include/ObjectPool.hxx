#pragma once

#ifndef OBJECTPOOL_HXX
#define OBJECTPOOL_HXX

#include "Compatibility.hxx"

#include <ace/Malloc_T.h>
#include <vector>

namespace Manager
{
    template<typename T>
    class ObjectPool
    {
        public:
            explicit ObjectPool (size_t capacity) : m_allocator (capacity),
                                                    m_capacity (capacity)
            {
                m_freeList.reserve (capacity);
            }

            ~ObjectPool()
            {
                clear();
            }

            T* acquire()
            {
                T* obj = static_cast<T*> (m_allocator.malloc());

                if (obj)
                {
                    new (obj) T();
                    m_freeList.push_back (obj);
                    return obj;
                }

                return nullptr;  // Signals "pool exhausted"
            }

            void release (T* obj)
            {
                if (!obj) return;

                obj->~T();
                m_allocator.free (obj);
            }

            size_t capacity() const
            {
                return m_capacity;
            }

            void clear()
            {
                for (size_t i = 0; i < m_freeList.size(); ++i)
                {
                    if (m_freeList[i]) m_freeList[i]->~T();
                }

                m_freeList.clear();
            }

        private:
            ACE_Cached_Allocator<T, ACE_Thread_Mutex> m_allocator;
            std::vector<T*> m_freeList;
            size_t m_capacity;
    };
} // namespace Manager

#endif // OBJECTPOOL_HXX
