//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_REFERENCE_COUNTED_HH
#define MIKOTO_REFERENCE_COUNTED_HH

#include <EASTL/atomic.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Logging/Assert.hh>

namespace mikoto::core {

    /**
     * @brief Base class for reference-counted objects.
     * https://isocpp.org/wiki/faq/freestore-mgmt#delete-this
     */
    template <class T>
    class ReferenceCounted {
    public:
        ReferenceCounted() noexcept = default;

        virtual ~ReferenceCounted() {
            MKT_ASSERT( mRefCount == 0u, "Object destroyed while references still exist!" );
        }

        auto AddRef() const noexcept -> void {
            mRefCount.fetch_add(1, eastl::memory_order_relaxed);
        }

        auto Release() const noexcept -> void {
#if !MIKOTO_TSAN_ENABLED
            if ( mRefCount.fetch_sub( 1, eastl::memory_order_release ) == 1 ) {
                eastl::atomic_thread_fence( eastl::memory_order_acquire );
                delete static_cast<const T*>( this );
            }
#else
            if ( mRefCount.fetch_sub( 1, eastl::memory_order_acq_rel ) == 1 ) {
                delete static_cast<const T*>( this );
            }
#endif
        }

        MKT_NODISCARD auto GetRefCount() const noexcept -> core::u32 {
            return mRefCount;
        }

    private:
        mutable eastl::atomic<core::u32> mRefCount{ 0 };
    };

    /**
    * @brief Base class for reference-counted objects.
    * https://isocpp.org/wiki/faq/freestore-mgmt#delete-this
    * */
    template<typename RefCountedType>
    class Ref {
    public:
        explicit Ref( RefCountedType* ptr = nullptr ) noexcept
            : mPtr{ ptr } {
            // RefCountedType must be a ReferenceCounted or inheriting from it
            // ReferenceCounted by default has the count set to one as the first usage counts
            if ( mPtr ) {
                mPtr->AddRef();
            }
        }

        Ref( Ref&& other ) noexcept
            : mPtr{ other.mPtr } {
            other.mPtr = nullptr;
        }

        Ref( const Ref& other )
            : mPtr{ other.mPtr } {
            if ( mPtr ) {
                mPtr->AddRef();
            }
        }

        auto operator=( Ref&& other ) noexcept -> Ref& {
            if ( mPtr != other.mPtr ) {
                // I need to free the implicit parameter first,
                // in case other.m_Ptr is different from m_Ptr
                if ( mPtr ) {
                    mPtr->Release();
                }

                mPtr = other.mPtr;

                other.mPtr = nullptr;
            }

            return *this;
        }

        auto operator=( const Ref& other ) -> Ref& {
            if ( this != std::addressof( other ) && mPtr != other.mPtr ) {
                // I need to free the implicit parameter first,
                // in case other.m_Ptr is different from m_Ptr
                if ( mPtr ) {
                    mPtr->Release();
                }

                mPtr = other.mPtr;

                // Then I call AddRef on the new pointer
                // to increase the ref count
                if ( mPtr ) {
                    mPtr->AddRef();
                }
            }

            return *this;
        }

        auto Reset() -> void {
            if ( mPtr ) {
                mPtr->Release();
                mPtr = nullptr;
            }
        }

        // This function was done for testing purposes is probably easy to use bad and does not solve anything for the time being
        // The idea came for cases when a class holds a standalone handle but we want to manually set to null the handle so that the inner ptr gets destroyed
        // problems is, think about calling new and then taking a reference from that pointer in two different Ref, that pointer will get freed by last handle
        // its dangerous to use the new-ed ptr anywhere after that.
        auto operator=( RefCountedType* ptr ) -> Ref& {
            if ( ptr == nullptr ) {
                if ( mPtr != nullptr ) {
                    mPtr->Release();
                }
            } else {
                ptr->AddRef();
            }

            mPtr = ptr;

            return *this;
        }

        MKT_NODISCARD auto operator==( RefCountedType* ptr ) const -> bool {
            return ptr == mPtr;
        }

        MKT_NODISCARD auto operator!=( RefCountedType* ptr ) const -> bool {
            return ptr != mPtr;
        }

        template<typename OtherRefCountedType>
        MKT_NODISCARD auto As() const -> Ref<OtherRefCountedType> {
            return Ref<OtherRefCountedType>( checked_cast<OtherRefCountedType*>( mPtr ) );
        }

        template<typename OtherRefCountedType>
        MKT_NODISCARD operator Ref<OtherRefCountedType>() const {
            return As<OtherRefCountedType>();
        }

        ~Ref() {
            if ( mPtr ) {
                mPtr->Release();
            }

            mPtr = nullptr;
        }

        /// Comparison

        MKT_NODISCARD auto IsEmpty() const -> bool { return mPtr == nullptr; }

        MKT_NODISCARD operator bool() const { return !IsEmpty(); }
        MKT_NODISCARD auto operator==(const Ref& other) const -> bool { return mPtr == other.mPtr; }

        /// Pointer accessors

        MKT_NODISCARD auto operator->() -> RefCountedType* { return mPtr; }
        MKT_NODISCARD auto operator->() const -> const RefCountedType* { return mPtr; }

        MKT_NODISCARD auto operator*() -> RefCountedType& { return *mPtr; }
        MKT_NODISCARD auto operator*() const -> const RefCountedType& { return *mPtr; }

        MKT_NODISCARD auto GetPtr() -> RefCountedType* { return mPtr; }
        MKT_NODISCARD auto GetPtr() const -> const RefCountedType* { return mPtr; }

        /// Fluent builders

        MKT_NODISCARD static auto CreateEmpty( ) -> Ref { return Ref{ nullptr }; }
        MKT_NODISCARD static auto Create( RefCountedType* ptr ) -> Ref { return Ref{ ptr }; }

        template<typename... Args>
        MKT_NODISCARD static auto New( Args&&... args ) -> Ref { return Ref{ new RefCountedType{ std::forward<Args>( args )... } }; }
    private:

        RefCountedType* mPtr{ nullptr };
    };

}// namespace Mikoto
#endif//MIKOTO_REFERENCE_COUNTED_HH
