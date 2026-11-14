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

#ifndef MIKOTO_CORE_REFERENCE_COUNTED_HH
#define MIKOTO_CORE_REFERENCE_COUNTED_HH

#include <new>
#include <typeinfo>
#include <typeindex>
#include <type_traits>

#include <EASTL/utility.h>
#include <EASTL/atomic.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/Profiler.hh>

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
            MKT_BEGIN_PROFILER();
            MKT_ASSERT( mRefCount == 0u, "Object destroyed while references still exist!" );


#if MIKOTO_DEBUG
            std::lock_guard lock{ mMutex };
            MKT_ASSERT( mOwners.empty(), "Reference-counted object destroyed with tracked owners!" );
            mOwners.clear();
#endif
        }

        auto AddRef() const noexcept -> void {
            mRefCount.fetch_add(1, eastl::memory_order_relaxed);

#if MIKOTO_DEBUG
            try {
                std::lock_guard lock{ mMutex };
                auto itFind{ mOwners.find( typeid(T) ) };
                if (itFind != mOwners.end()) {
                    itFind->second.second += 1;
                } else {
                    auto str{ string::Format( "Object Type {} Address {:x}",
                        typeid(T).name(), rc_cast<core::ptr>(this) ) };
                    mOwners[typeid(T)] = eastl::make_pair( str, 1 );
                }
            } catch ( ... ) {

            }
#endif
        }

        auto Release() const noexcept -> void {
#if MIKOTO_DEBUG
            try {
                std::lock_guard lock{ mMutex };
                auto itFind{ mOwners.find( typeid(T) ) };
                if (itFind != mOwners.end()) {
                    itFind->second.second -= 1u;

                    if (itFind->second.second == 0u) {
                        mOwners.erase( itFind );
                    }
                }
            } catch (...) {
            }
#endif

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

#if MIKOTO_DEBUG
        mutable std::mutex mMutex{};
        mutable ankerl::unordered_dense::map<std::type_index, eastl::pair<eastl::string, core::u32>> mOwners{};
#endif
    };

    /**
    * @brief Base class for reference-counted objects.
    * https://isocpp.org/wiki/faq/freestore-mgmt#delete-this
    * */
    template<typename RefCountedType>
    class Ref {
    public:
        Ref( RefCountedType* ptr = nullptr ) noexcept
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

        MKT_NODISCARD auto IsEmpty() const -> bool {
            return mPtr == nullptr;
        }

        MKT_NODISCARD operator bool() const {
            return !IsEmpty();
        }
        MKT_NODISCARD auto operator==( const Ref& other ) const -> bool {
            return mPtr == other.mPtr;
        }

        /// Pointer accessors

        MKT_NODISCARD auto operator->() -> RefCountedType* {
            return mPtr;
        }
        MKT_NODISCARD auto operator->() const -> const RefCountedType* {
            return mPtr;
        }

        MKT_NODISCARD auto operator*() -> RefCountedType& {
            return *mPtr;
        }
        MKT_NODISCARD auto operator*() const -> const RefCountedType& {
            return *mPtr;
        }

        MKT_NODISCARD auto GetPtr() -> RefCountedType* {
            return mPtr;
        }
        MKT_NODISCARD auto GetPtr() const -> const RefCountedType* {
            return mPtr;
        }

        /// Fluent builders

        MKT_NODISCARD static auto CreateEmpty() -> Ref {
            return Ref{ nullptr };
        }
        MKT_NODISCARD static auto Create( RefCountedType* ptr ) -> Ref {
            return Ref{ ptr };
        }

        template<typename... Args>
        MKT_NODISCARD static auto New( Args&&... args ) -> Ref {
            return Ref{ new (std::nothrow) RefCountedType{ eastl::forward<Args>( args )... } };
        }

    private:

        RefCountedType* mPtr{ nullptr };
    };

    /**
     * @brief Shares ownership while exposing the object through const access.
     * @tparam RefCountedType Intrusively reference-counted object type.
     */
    template<typename RefCountedType>
    class RefConst {
    public:
        RefConst( const RefCountedType* ptr = nullptr ) noexcept
            : mPtr{ ptr } {
            // RefCountedType must be a ReferenceCounted or inheriting from it
            // ReferenceCounted by default has the count set to one as the first usage counts
            if ( mPtr ) {
                mPtr->AddRef();
            }
        }

        /**
         * @brief Retains a read-only view of an existing mutable reference.
         * @tparam Other Object type convertible to this reference's const object type.
         * @param other Owner whose object is shared without changing its access rights.
         */
        template<typename Other>
            requires std::is_convertible_v<const Other*, const RefCountedType*>
        RefConst( const Ref<Other>& other ) noexcept
            : RefConst{ other.GetPtr() } {
        }

        RefConst( RefConst&& other ) noexcept
            : mPtr{ other.mPtr } {
            other.mPtr = nullptr;
        }

        RefConst( const RefConst& other )
            : mPtr{ other.mPtr } {
            if ( mPtr ) {
                mPtr->AddRef();
            }
        }

        auto operator=( RefConst&& other ) noexcept -> RefConst& {
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

        auto operator=( const RefConst& other ) -> RefConst& {
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

        auto operator=( const RefCountedType* ptr ) -> RefConst& {
            if ( mPtr != ptr ) {
                if ( ptr ) {
                    ptr->AddRef();
                }
                if ( mPtr ) {
                    mPtr->Release();
                }
                mPtr = ptr;
            }

            return *this;
        }

        MKT_NODISCARD auto operator==( const RefCountedType* ptr ) const -> bool {
            return ptr == mPtr;
        }

        MKT_NODISCARD auto operator!=( const RefCountedType* ptr ) const -> bool {
            return ptr != mPtr;
        }

        template<typename OtherRefCountedType>
        MKT_NODISCARD auto As() const -> RefConst<OtherRefCountedType> {
            return RefConst<OtherRefCountedType>( checked_cast<const OtherRefCountedType*>( mPtr ) );
        }

        template<typename OtherRefCountedType>
        MKT_NODISCARD operator RefConst<OtherRefCountedType>() const {
            return As<OtherRefCountedType>();
        }

        ~RefConst() {
            if ( mPtr ) {
                mPtr->Release();
            }

            mPtr = nullptr;
        }

        /// Comparison

        MKT_NODISCARD auto IsEmpty() const -> bool {
            return mPtr == nullptr;
        }

        MKT_NODISCARD operator bool() const {
            return !IsEmpty();
        }
        MKT_NODISCARD auto operator==( const RefConst& other ) const -> bool {
            return mPtr == other.mPtr;
        }

        /// Pointer accessors

        MKT_NODISCARD auto operator->() const -> const RefCountedType* {
            return mPtr;
        }

        MKT_NODISCARD auto operator*() const -> const RefCountedType& {
            return *mPtr;
        }

        MKT_NODISCARD auto GetPtr() const -> const RefCountedType* {
            return mPtr;
        }

        /// Fluent builders

        MKT_NODISCARD static auto CreateEmpty() -> RefConst {
            return RefConst{ nullptr };
        }

        MKT_NODISCARD static auto Create( const RefCountedType* ptr ) -> RefConst {
            return RefConst{ ptr };
        }

        template<typename... Args>
        MKT_NODISCARD static auto New( Args&&... args ) -> RefConst {
            return RefConst{ new RefCountedType{ eastl::forward<Args>( args )... } };
        }

    private:

        const RefCountedType* mPtr{ nullptr };
    };

}// namespace Mikoto

#endif// MIKOTO_CORE_REFERENCE_COUNTED_HH
