//
// Created by zanet on 4/9/2025.
//

#ifndef REFERENCECOUNTED_HH
#define REFERENCECOUNTED_HH

#include <atomic>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Logging/Assert.hh>
#include <functional>

namespace Mikoto
{
    /**
     * @brief Base class for reference-counted objects.
     *
     * Designed for intrusive smart pointers in the engine.
     * Users must call AddRef() when retaining a reference and Release() when releasing one.
     * Object must have been created by new
     * https://isocpp.org/wiki/faq/freestore-mgmt#delete-this
     */
    class ReferenceCounted
    {
    public:
        ReferenceCounted() noexcept = default;

        // Prevent copy/move
        DISABLE_COPY_AND_MOVE_FOR(ReferenceCounted);

        virtual ~ReferenceCounted()
        {
            MKT_ASSERT(m_RefCount == 0, "Object destroyed while references still exist!");
        }

        auto Acquire() const noexcept -> void
        {
#if MIKOTO_THREAD_SAFE_REFS
            m_RefCount.fetch_add(1, std::memory_order_relaxed);
#else
            ++m_RefCount;
#endif
        }

        auto Free() const noexcept -> void
        {
#if MIKOTO_THREAD_SAFE_REFS
            if (m_RefCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                delete this;
            }
#else

            if (const UInt32 count{--m_RefCount}; count == 0)
            {
                delete this;
            }
#endif
        }

        auto GetRefCount() const noexcept -> UInt32
        {
#if MIKOTO_THREAD_SAFE_REFS
            return m_RefCount.load(std::memory_order_relaxed);
#else
            return m_RefCount;
#endif
        }

    protected:
        // For debug builds: allow identifying the object
        virtual auto GetDebugName() const noexcept -> CStr { return "ReferenceCountedObject"; }

    private:
#if MIKOTO_THREAD_SAFE_REFS
        mutable std::atomic<UInt32_T> m_RefCount{ 1 };
#else
        mutable UInt32 m_RefCount{1};
#endif
    };

    /**
       * @brief Base class for reference-counted objects.
       *
       * Designed for intrusive smart pointers in the engine.
       * Users must call AddRef() when retaining a reference and Release() when releasing one.
       * Child must implement auto FreeObject() -> void;
       * https://isocpp.org/wiki/faq/freestore-mgmt#delete-this
       */


    template <typename RefCountedType>
    class Ref
    {
    public:
        explicit Ref(RefCountedType* ptr = nullptr)
            : m_Ptr(ptr)
        {
            if (m_Ptr) m_Ptr->Acquire();
        }

        Ref(Ref&& other)
            noexcept : m_Ptr(other.m_Ptr)
        {
            if (m_Ptr)
            {
                m_Ptr->Acquire();
            }

            m_Ptr->Free();
        }

        Ref& operator=(const Ref& other)
        {
            if (m_Ptr != other.m_Ptr)
            {
                // I need to free the implicit parameter first,
                // in case other.m_Ptr is the same as m_Ptr
                if (m_Ptr)
                {
                    m_Ptr->Free();
                }

                m_Ptr = other.m_Ptr;

                // Then I call Acquire on the new pointer
                // to increase the ref count
                if (m_Ptr)
                {
                    m_Ptr->Acquire();
                }
            }
            return *this;
        }

        template <typename OtherRefCountedType>
        auto As() const -> Ref<OtherRefCountedType>
        {
            if constexpr (!std::is_same_v<RefCountedType, OtherRefCountedType>)
            {
                return nullptr;
            }

            return Ref<OtherRefCountedType>(static_cast<OtherRefCountedType*>(m_Ptr));
        }

        ~Ref()
        {
            if (m_Ptr)
            {
                m_Ptr->Free();
            }
        }

        explicit operator RefCountedType*()
        {
            return m_Ptr;
        }

        MKT_NODISCARD auto IsEmpty() const -> bool
        {
            return !m_Ptr;
        }

        static auto Create(RefCountedType* ptr) -> Ref<RefCountedType>
        {
            return Ref<RefCountedType>{ptr};
        }

        static auto CreateEmpty() -> Ref<RefCountedType>
        {
            return Ref<RefCountedType>{nullptr};
        }

        RefCountedType* operator->() { return m_Ptr; }
        const RefCountedType* operator->() const { return m_Ptr; }

        RefCountedType* Get() { return m_Ptr; }
        const RefCountedType* Get() const { return m_Ptr; }

    private:
        RefCountedType* m_Ptr{nullptr};
    };
} // namespace Mikoto
#endif //REFERENCECOUNTED_HH
