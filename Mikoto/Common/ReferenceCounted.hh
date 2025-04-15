//
// Created by zanet on 4/9/2025.
//

#ifndef REFERENCECOUNTED_HH
#define REFERENCECOUNTED_HH

#include <atomic>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Core/Assert.hh>

namespace Mikoto {

    /**
     * @brief Base class for reference-counted objects.
     *
     * Designed for intrusive smart pointers in the engine.
     * Users must call AddRef() when retaining a reference and Release() when releasing one.
     * Object must have been created by new
     * https://isocpp.org/wiki/faq/freestore-mgmt#delete-this
     */
    class ReferenceCounted {
    public:
        ReferenceCounted() noexcept = default;

        // Prevent copy/move
        DISABLE_COPY_AND_MOVE_FOR( ReferenceCounted );

        virtual ~ReferenceCounted() {
            MKT_ASSERT(m_RefCount == 0, "Object destroyed while references still exist!");
        }

        auto Acquire() const noexcept -> void {
#if MIKOTO_THREAD_SAFE_REFS
            m_RefCount.fetch_add(1, std::memory_order_relaxed);
#else
            ++m_RefCount;
#endif
        }

        auto Release() const noexcept -> void {
#if MIKOTO_THREAD_SAFE_REFS
            if (m_RefCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                delete this;
            }
#else
            UInt32_T count{ --m_RefCount };

            if (count == 0) {
                delete this;
            }
#endif
        }

        auto GetRefCount() const noexcept -> UInt32_T {
#if MIKOTO_THREAD_SAFE_REFS
            return m_RefCount.load(std::memory_order_relaxed);
#else
            return m_RefCount;
#endif
        }

    protected:

        // For debug builds: allow identifying the object
        virtual auto GetDebugName() const noexcept -> CStr_T { return "ReferenceCountedObject"; }

    private:
#if MIKOTO_THREAD_SAFE_REFS
        mutable std::atomic<UInt32_T> m_RefCount{ 1 };
#else
        mutable UInt32_T m_RefCount{ 1 };
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
    template<typename T>
  class ReferenceCountedTyped {
  public:
    ReferenceCountedTyped() noexcept = default;

    // Prevent copy/move
    DISABLE_COPY_AND_MOVE_FOR( ReferenceCountedTyped );

    virtual ~ReferenceCountedTyped() {
      MKT_ASSERT(m_RefCount == 0, "Object destroyed while references still exist!");
    }

    auto Acquire() const noexcept -> void {
#if MIKOTO_THREAD_SAFE_REFS
      m_RefCount.fetch_add(1, std::memory_order_relaxed);
#else
      ++m_RefCount;
#endif
    }

    auto Release() const noexcept -> void {
#if MIKOTO_THREAD_SAFE_REFS
      if (m_RefCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete this;
      }
#else
      UInt32_T count{ --m_RefCount };

      if (count == 0) {
        Free();
      }
#endif
    }

    auto GetRefCount() const noexcept -> UInt32_T {
#if MIKOTO_THREAD_SAFE_REFS
      return m_RefCount.load(std::memory_order_relaxed);
#else
      return m_RefCount;
#endif
    }

  protected:
        auto Free() -> void {
            As<T*>(this)->FreeObject();
        }

    // For debug builds: allow identifying the object
    virtual auto GetDebugName() const noexcept -> CStr_T { return "ReferenceCountedObject"; }

  private:
#if MIKOTO_THREAD_SAFE_REFS
    mutable std::atomic<UInt32_T> m_RefCount{ 1 };
#else
    mutable UInt32_T m_RefCount{ 1 };
#endif
  };

    template<typename RefCountedType>
class Ref {
    public:
        explicit Ref() = default;

        explicit Ref(RefCountedType* ptr)
            : m_Ptr(ptr) {
            if (m_Ptr) m_Ptr->Acquire();
        }

        Ref(const Ref& other)
            : m_Ptr(other.m_Ptr) {
            if (m_Ptr) {
                m_Ptr->Acquire();
            }
        }

        Ref& operator=(const Ref& other) {
            if (m_Ptr != other.m_Ptr) {
                if (m_Ptr) {
                    m_Ptr->Release();
                }

                m_Ptr = other.m_Ptr;

                if (m_Ptr) {
                    m_Ptr->Acquire();
                }
            }
            return *this;
        }

        template<typename OtherRefCountedType>
        auto As() const -> Ref<OtherRefCountedType> {
            if constexpr (std::is_same_v<RefCountedType, OtherRefCountedType>) {
                return nullptr;
            }

            return Ref<OtherRefCountedType>(Cast<OtherRefCountedType*>(m_Ptr));
        }

        ~Ref() {
            if (m_Ptr) {
                m_Ptr->Release();
            }
        }

        explicit operator RefCountedType*() {
            return m_Ptr;
        }

        RefCountedType* operator->() { return m_Ptr; }
        const RefCountedType* operator->() const { return m_Ptr; }

        RefCountedType* Get() { return m_Ptr; }
        const RefCountedType* Get() const { return m_Ptr; }

    private:
        RefCountedType* m_Ptr{ nullptr };
    };

    class RefAny {
    public:
        RefAny() = default;

        template<typename T>
        explicit RefAny(Ref<T> ref)
            : m_Ptr(ref.Get())
            , m_Deleter([ref]() mutable {})  // Keeps ownership alive
            , m_Type(typeid(T)) {}

        template<typename T>
        auto As() const -> Ref<T> {
            if (m_Type != typeid(T)) return nullptr;
            return Ref<T>(static_cast<T*>(m_Ptr));
        }

        auto Raw() const -> void* { return m_Ptr; }

        auto Type() const -> const std::type_info& { return m_Type; }

    private:
        void* m_Ptr{nullptr};

        // Capture the Ref<T> to extend its lifetime until RefAny is destroyed
        std::function<void()> m_Deleter{};

        const std::type_info& m_Type{ typeid(void) };
    };

} // namespace Mikoto
#endif //REFERENCECOUNTED_HH
