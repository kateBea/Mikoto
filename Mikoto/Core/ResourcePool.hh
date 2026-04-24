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

#ifndef MIKOTO_RESOURCE_POOL_HH
#define MIKOTO_RESOURCE_POOL_HH

#include <ranges>

#include <EASTL/atomic.h>
#include <EASTL/vector.h>
#include <EASTL/utility.h>
#include <EASTL/numeric.h>
#include <EASTL/type_traits.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/ReferenceCounted.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

namespace mikoto::core {

    using Handle = u32;

    constexpr Handle kInvalidHandle{ ( eastl::numeric_limits<Handle>::max )() };


    MKT_NODISCARD constexpr auto IsValidHandle( const Handle handle ) -> bool {
        return handle != kInvalidHandle;
    }

    class IResource : public core::ReferenceCounted {
    public:

        MKT_NODISCARD auto IsUsed() const -> bool { return GetRefCount() != 0; }
        MKT_NODISCARD auto GetHandle() const -> Handle { return mHandle; }
        MKT_NODISCARD auto IsReady() const -> bool { return mIsAllocated; }

        auto SetHandle( const Handle value ) -> void { mHandle = value; }
        auto SetIsReady(const bool value) -> void { mIsAllocated = value; }

    protected:
        virtual auto Initialize() -> void = 0;
        virtual auto Release() -> void = 0;

    protected:
        Handle mHandle{};
        bool mIsAllocated{ false };
    };

    /**
    * @class ResourcePool
    * @brief Manages a pool of reusable resources [Internal].
    * This class is meant to support ResourcePoolType, do not use it
    *
    * The `ResourcePool` class provides an efficient way to manage resources,
    * allowing to reuse and minimizing allocations. It is designed to store and
    * retrieve resources dynamically as needed. `IResource`'s registered to a resource pool
    * do need to be free-d by the user, the pool does not take care of deallocating resources
    */
    class ResourcePool {
    public:
        using ResourceHandle = Ref<IResource>;

        /**
        * @brief Initializes the resource pool.
        * @param poolSize The number of resources the pool can hold.
        */
        auto Init( const u32 poolSize ) -> void {
            mPoolSize = poolSize;
            mFreeSlots.reserve( poolSize );

            for ( Handle i{ 0 }; i < poolSize; ++i ) {
                mFreeSlots.emplace_back( i );
            }

            mResources.reserve( poolSize );
        }

        /**
        * @brief Shuts down the resource pool, releasing all allocated resources.
        */
        auto Shutdown() -> void {
            mFreeSlots.clear();
            mResources.clear();
        }

        auto Clear() -> void {

            mFreeSlots.clear();
            mResources.clear();

            Init(mPoolSize);
        }

    protected:
        auto ReleaseResource( const Handle index ) -> void {
            const auto it{ mResources.find(index) };

            if (it->second.IsEmpty()) {
                return;
            }

            if ( it != mResources.end() ) {
                it->second = ResourceHandle::CreateEmpty();
                mFreeSlots.emplace_back(index);
            }
        }

        /**
        * @brief Accesses a resource by its index.
        * @param index The index of the resource to access.
        * @return A pointer to the resource, or nullptr if invalid.
        */
        MKT_NODISCARD auto AccessResource( const Handle index ) const -> ResourceHandle {
            return mResources.contains(index) ? mResources.at(index) : ResourceHandle::CreateEmpty();
        }

    protected:
        /**
        * @brief Gets a resource slot from the pool.
        * @return The index of the obtained resource, or `INVALID_HANDLE` if allocation fails.
        */
        MKT_NODISCARD auto ObtainResource() -> Handle {
            const Handle index{ mFreeSlots.back() };
            mFreeSlots.pop_back();

            return index;
        }

        auto Resize() -> void {
            // When pool is full free handles will be empty

            // Add the other handles after current limit size
            // Last available handle ID is m_PoolSize - 1 (see init)
            for (u32 i{}; i <  (mPoolSize * POOL_RESIZE_RATE) - mPoolSize; ++i ) {
                mFreeSlots.emplace_back( i + mPoolSize );
            }

            mPoolSize = mPoolSize + mFreeSlots.size();
        }

        MKT_NODISCARD auto IsPoolFull() const -> bool { return mFreeSlots.size() == 0; }

    protected:
        static constexpr double POOL_RESIZE_RATE{ 2.5 };

    protected:
        u32 mPoolSize{ 100 };
        eastl::vector<Handle> mFreeSlots{};
        ankerl::unordered_dense::map<Handle, ResourceHandle> mResources{};
    };

    /**
    * @class ResourcePoolTyped
    * @brief A type-safe resource pool for managing a specific resource type.
    *
    * @tparam T The resource type, must inherit from `IResource`.
    */
    template<typename T>
    class ResourcePoolTyped final : public ResourcePool {
    public:
        using Pointer = T*;
        using RefHandle = Ref<T>;

        /**
        * @brief Collects a new typed resource from the pool.
        *
        * This function retrieves a free resource slot from the pool. If the resource
        * does not already exist at the given index, it is created using the provided
        * arguments, but it is not initialized (no call to Allocate).
        *
        * @tparam Args Variadic template parameters for forwarding arguments to the resource constructor.
        * @param args Arguments to construct the resource if it does not already exist.
        * @return A pointer to the obtained resource, or nullptr if allocation fails.
        */
        template<typename... Args>
        MKT_NODISCARD auto Allocate(Args&&... args) -> RefHandle {
            if (IsPoolFull()) {
                Resize();
            }

            const u32 index{ ObtainResource() };

            Pointer ptr{ new (std::nothrow) T(eastl::forward<Args>(args)... ) };
            ResourceHandle newResource{ ResourceHandle::Create(ptr) };

            mResources[index] = newResource;
            mResources[index]->SetHandle( index );

            return mResources[index];
        }

        auto RemoveOrphans() -> void {
            for (const auto& resource : mResources | std::views::values ) {
                if (resource.IsEmpty()) {
                    continue;
                }

                if (resource->GetRefCount() == 1) {
                    ReleaseResource( resource->GetHandle() );
                }
            }
        }

        /**
         * @brief Releases a typed resource back to the pool.
         * @param handle The handle of the resource to be released.
         */
        auto Release( const Handle handle ) -> void {
            ReleaseResource( handle );
        }

        /**
         * @brief Gets a typed resource by its index.
         * @param index The index of the resource to access.
         * @return A pointer to the resource, or nullptr if invalid.
         */
        MKT_NODISCARD auto Get( const Handle index ) -> RefHandle {
            if ( auto result{ AccessResource( index ) }; !result.IsEmpty()) {
                return result.As<T>();
            }

            return RefHandle::CreateEmpty();
        }

        MKT_NODISCARD auto GetResource() -> RefHandle {
            // This function can be used to get the first available resource
            // we might preallocate and have them ready for usage later
            RefHandle result{ RefHandle::CreateEmpty() };

            // Find the first available resource
            for (ResourceHandle& resource : mResources  | std::views::values ) {
                if (!resource.IsEmpty() && resource->IsReady()) {
                    result = resource.As<T>();
                    break;
                }
            }

            return result;
        }

        auto begin() { return mResources.begin(); }
        auto end() { return mResources.end(); }

        auto cbegin() const { return mResources.cbegin(); }
        auto cend() const { return mResources.cend(); }
    };
}

#endif// MIKOTO_RESOURCE_POOL_HH
