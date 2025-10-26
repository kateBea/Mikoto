//
// Created by zanet on 4/9/2025.
//

#ifndef REFERENCECOUNTED_HH
#define REFERENCECOUNTED_HH

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Logging/Assert.hh>
#include <atomic>
#include <functional>

namespace Mikoto {
    /**
     * @brief Base class for reference-counted objects.
     * https://isocpp.org/wiki/faq/freestore-mgmt#delete-this
     */
    class ReferenceCounted {
    public:
        ReferenceCounted() noexcept = default;

        // Prevent copy/move
        DISABLE_COPY_AND_MOVE_FOR( ReferenceCounted );

        virtual ~ReferenceCounted() {
            MKT_ASSERT( m_RefCount == 0, "Object destroyed while references still exist!" );
        }

        auto Acquire() const noexcept -> void {
            ++m_RefCount;
        }

        auto Free() const noexcept -> void {

            if ( const UInt32 count{ --m_RefCount }; count == 0 ) {
                delete this;
            }
        }

        auto GetRefCount() const noexcept -> UInt32 {
            return m_RefCount;
        }

        mutable UInt32 m_RefCount{ 0 };
    };

    /**
    * @brief Base class for reference-counted objects.
    * https://isocpp.org/wiki/faq/freestore-mgmt#delete-this
    */
    template<typename RefCountedType>
    class Ref {
    public:
        explicit Ref( RefCountedType* ptr = nullptr ) noexcept
            : m_Ptr{ ptr } {
            // RefCountedType must be a ReferenceCounted or inheriting from it
            // ReferenceCounted by default has the count set to one as the first usage counts
            if (m_Ptr) {
                m_Ptr->Acquire();
            }
        }

        Ref( Ref&& other ) noexcept : m_Ptr( other.m_Ptr ) {
            other.m_Ptr = nullptr;
        }

        Ref( const Ref& other )
            : m_Ptr( other.m_Ptr ) {
            if ( m_Ptr ) {
                m_Ptr->Acquire();
            }
        }

        auto operator=( Ref&& other ) noexcept -> Ref& {
            if ( m_Ptr != other.m_Ptr ) {
                // I need to free the implicit parameter first,
                // in case other.m_Ptr is different from m_Ptr
                if ( m_Ptr ) {
                    m_Ptr->Free();
                }

                m_Ptr = other.m_Ptr;

                other.m_Ptr = nullptr;
            }

            return *this;
        }

        auto operator=( const Ref& other ) -> Ref& {
            if ( m_Ptr != other.m_Ptr ) {
                // I need to free the implicit parameter first,
                // in case other.m_Ptr is different from m_Ptr
                if ( m_Ptr ) {
                    m_Ptr->Free();
                }

                m_Ptr = other.m_Ptr;

                // Then I call Acquire on the new pointer
                // to increase the ref count
                if ( m_Ptr ) {
                    m_Ptr->Acquire();
                }
            }
            return *this;
        }

        auto Disable() -> void {
            if (m_Ptr) {
                m_Ptr->Free();
                m_Ptr = nullptr;
            }
        }

        // This function was done for testing purposes is probably easy to use bad and does not solve anything for the time being
        // The idea came for cases when a class holds a standalone handle but we want to manually set to null the handle so that the inner ptr gets destroyed
        // problems is, think about calling new and then taking a reference from that pointer in two different Ref, that pointer will get freed by last handle
        // its dangerous to use the new-ed ptr anywhere after that.
        auto operator=(RefCountedType* ptr) -> Ref& {
            if (ptr == nullptr) {
                if (m_Ptr != nullptr) {
                    m_Ptr->Free();
                }
            } else {
                ptr->Acquire();
            }

            m_Ptr = ptr;

            return *this;
        }

        template<typename OtherRefCountedType>
        auto As() const -> Ref<OtherRefCountedType> {
            // if constexpr ( RelatedDynamicallyCastable<OtherRefCountedType*, RefCountedType*> == true ) {
            //     return Ref<OtherRefCountedType>::CreateEmpty();
            // }

            return Ref<OtherRefCountedType>( dynamic_cast<OtherRefCountedType*>( m_Ptr ) );
        }

        ~Ref() {
            if ( m_Ptr ) {
                m_Ptr->Free();
            }

            m_Ptr = nullptr;
        }

        explicit operator RefCountedType*() {
            return m_Ptr;
        }

        MKT_NODISCARD auto IsEmpty() const -> bool { return m_Ptr == nullptr; }

        MKT_NODISCARD static auto CreateEmpty() -> Ref { return Ref{ nullptr }; }
        MKT_NODISCARD static auto Create( RefCountedType* ptr ) -> Ref { return Ref{ ptr }; }

        auto operator->() -> RefCountedType* { return m_Ptr; }
        auto operator->() const -> const RefCountedType* { return m_Ptr; }

        auto GetRaw() -> RefCountedType* { return m_Ptr; }
        auto GetRaw() const -> const RefCountedType* { return m_Ptr; }

    private:
        friend class ReferenceCounted;

        RefCountedType* m_Ptr{ nullptr };
    };

}// namespace Mikoto
#endif//REFERENCECOUNTED_HH
