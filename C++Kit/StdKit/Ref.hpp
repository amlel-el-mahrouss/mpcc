
/*
 *	========================================================
 *
 *	NewOS
 * 	Copyright Amlal El Mahrouss, all rights reserved.
 *
 * 	========================================================
 */

#pragma once

#include <C++Kit/Defines.hpp>

namespace CxxKit
{
    // @author Amlal El Mahrouss
    // @brief Reference class, refers to a pointer of data in static memory.
    template <typename T>
    class Ref final
    {
    public:
        Ref() = default;
        ~Ref() = default;

    public:
        Ref(T cls, const bool &strong = false) : m_Class(cls), m_Strong(strong) {}

        Ref &operator=(T ref)
        {
            m_Class = ref;
            return *this;
        }

    public:
        T operator->() const
        {
            return m_Class;
        }

        T &Leak()
        {
            return m_Class;
        }

        T operator*()
        {
            return m_Class;
        }

        bool IsStrong() const
        {
            return m_Strong;
        }

        operator bool()
        {
            return m_Class;
        }

    private:
        T m_Class;
        bool m_Strong{ false };

    };

    template <typename T>
    class NonNullRef final
    {
    public:
        NonNullRef() = delete;
        NonNullRef(nullPtr) = delete;

        NonNullRef(T *ref) : m_Ref(ref, true) {}

        Ref<T> &operator->()
        {
            MUST_PASS(m_Ref);
            return m_Ref;
        }

        NonNullRef &operator=(const NonNullRef<T> &ref) = delete;
        NonNullRef(const NonNullRef<T> &ref) = default;

    private:
        Ref<T> m_Ref{ nullptr };
        
    };
} // namespace CxxKit
