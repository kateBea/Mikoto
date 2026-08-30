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
#include <EASTL/functional.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Logging/Assert.hh>

namespace mikoto::core {
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
            MKT_ASSERT( mRefCount == 0u, "Object destroyed while references still exist!" );
        }

        auto Acquire() const noexcept -> void {
            ++mRefCount;
        }

        auto Free() const noexcept -> void {

            if ( const u32 count{ --mRefCount }; count == 0 ) {
                delete this;
            }
        }

        auto GetRefCount() const noexcept -> u32 {
            return mRefCount;
        }

    
    private:

        mutable eastl::atomic<u32> mRefCount{ 0 };
    };

    /**
    * @brief Base class for reference-counted objects.
    * https://isocpp.org/wiki/faq/freestore-mgmt#delete-this
    */
    template<typename RefCountedType>
    class Ref {
    public:
        explicit Ref( RefCountedType* ptr = nullptr ) noexcept
            : mPtr{ ptr } {
            // RefCountedType must be a ReferenceCounted or inheriting from it
            // ReferenceCounted by default has the count set to one as the first usage counts
            if ( mPtr ) {
                mPtr->Acquire();
            }
        }

        Ref( Ref&& other ) noexcept : mPtr( other.mPtr ) {
            other.mPtr = nullptr;
        }

        Ref( const Ref& other )
            : mPtr( other.mPtr ) {
            if ( mPtr ) {
                mPtr->Acquire();
            }
        }

        auto operator=( Ref&& other ) noexcept -> Ref& {
            if ( mPtr != other.mPtr ) {
                // I need to free the implicit parameter first,
                // in case other.m_Ptr is different from m_Ptr
                if ( mPtr ) {
                    mPtr->Free();
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
                    mPtr->Free();
                }

                mPtr = other.mPtr;

                // Then I call Acquire on the new pointer
                // to increase the ref count
                if ( mPtr ) {
                    mPtr->Acquire();
                }
            }

            return *this;
        }

        auto Release() -> void {
            if ( mPtr ) {
                mPtr->Free();
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
                    mPtr->Free();
                }
            } else {
                ptr->Acquire();
            }

            mPtr = ptr;

            return *this;
        }

        auto operator==( RefCountedType* ptr ) const -> bool {
            return ptr == mPtr;
        }

        auto operator!=( RefCountedType* ptr ) const -> bool {
            return ptr != mPtr;
        }

        template<typename OtherRefCountedType>
        auto As() const -> Ref<OtherRefCountedType> {
            return Ref<OtherRefCountedType>( checked_cast<OtherRefCountedType*>( mPtr ) );
        }

        template<typename OtherRefCountedType>
        operator Ref<OtherRefCountedType>() const {
            return As<OtherRefCountedType>();
        }

        ~Ref() {
            if ( mPtr ) {
                mPtr->Free();
            }

            mPtr = nullptr;
        }

        MKT_NODISCARD auto IsEmpty() const -> bool { return mPtr == nullptr; }

        MKT_NODISCARD static auto CreateEmpty() -> Ref { return Ref{ nullptr }; }
        MKT_NODISCARD static auto Create( RefCountedType* ptr ) -> Ref { return Ref{ ptr }; }

        template<typename... Args>
        MKT_NODISCARD static auto New( Args&&... args ) -> Ref { return Ref{ new RefCountedType{ std::forward<Args>( args )... } }; }

        auto operator->() -> RefCountedType* { return mPtr; }
        auto operator->() const -> const RefCountedType* { return mPtr; }

        auto operator*() -> RefCountedType& { return *mPtr; }
        auto operator*() const -> const RefCountedType& { return *mPtr; }

        auto operator==(const Ref& other) const -> bool { return mPtr == other.mPtr; }

        operator bool() const {
            return !IsEmpty();
        }

        auto GetRaw() -> RefCountedType* { return mPtr; }
        auto GetRaw() const -> const RefCountedType* { return mPtr; }

        template<typename T>
        auto Dynamic() -> decltype(auto) { return dynamic_cast<T*>(mPtr); }

    private:
        friend class ReferenceCounted;

        RefCountedType* mPtr{ nullptr };
    };

}// namespace Mikoto
#endif//MIKOTO_REFERENCE_COUNTED_HH
