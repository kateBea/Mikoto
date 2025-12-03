//
// Created by zanet on 3/27/2025.
//

#ifndef RESOURCEPOOL_HH
#define RESOURCEPOOL_HH

#include <ranges>
#include <type_traits>
#include <unordered_map>

#include <ankerl/unordered_dense.h>

#include <Common/Common.hh>
#include <Common/ReferenceCounted.hh>
#include <Library/Utility/Types.hh>
#include <Logging/Assert.hh>
#include <Logging/Logger.hh>
#include <Memory/HeapAllocator.hh>

namespace Mikoto {

    /**
     * @typedef Handle
     * @brief Alias for a resource handle type.
     *
     * This type is used to uniquely identify resources managed by a pool.
     * It is defined as a `Size_T` type, and `INVALID_HANDLE` represents an invalid resource handle.
     */
    using Handle = UInt32;

    /**
     * @var INVALID_HANDLE
     * @brief Invalid handle value.
     *
     * This constant is used to represent an invalid or uninitialized resource handle.
     * It is defined as the maximum value of the `Handle` type, indicating that a resource handle is invalid.
     */
    constexpr Handle INVALID_HANDLE{ ( std::numeric_limits<Handle>::max )() };

    /**
    * @brief Checks whether a given handle is valid.
    *
    * A handle is considered valid if it is not equal to `INVALID_HANDLE`.
    * Helper to verify whether a resource handle
    * has been properly assigned before usage.
    *
    * @param handle The handle to check.
    * @return True if the handle is valid, false otherwise.
    */
    constexpr auto IsValidHandle( const Handle handle ) -> bool {
        return handle != INVALID_HANDLE;
    }

    /**
    * @class IResource
    * @brief Abstract base class for resources managed by a `ResourcePool`.
    *
    * The `IResource` class serves as the interface for resources that are managed and pooled within a resource management system.
    * Derived classes must implement the `Allocate` and `Release` methods to define how the resource is allocated and freed.
    * This structure allows resources to be dynamically managed by a resource pool, ensuring efficient allocation and cleanup.
    */
    class IResource : public ReferenceCounted {
    public:
#if !defined(NDEBUG)
        IResource() {
            ++s_ResourceCount;
            //MKT_CORE_LOGGER_DEBUG("Creating resource, count is no: {}", s_ResourceCount );
        };


        ~IResource() override {
            --s_ResourceCount;
            //MKT_CORE_LOGGER_DEBUG("Destroying resource, count is no: {}", s_ResourceCount );
        };

#else

        ~IResource() override = default;

#endif
        /**
        * @brief Retrieves the handle associated with the resource.
        *
        * Returns the handle (ID) that uniquely identifies the resource in the pool.
        *
        * @return The resource's handle.
        */
        MKT_NODISCARD auto GetHandle() const -> Handle { return m_Handle; }

        /**
        * @brief Checks whether the resource is allocated and ready for use.
        *
        * This method determines if the resource is currently allocated and
        * available for usage.
        *
        * @return `true` if the resource is allocated, otherwise `false`.
        */
        MKT_NODISCARD auto IsReady() const -> bool { return m_IsAllocated; }

        /**
        * @brief Sets the handle for the resource.
        *
        * Allows setting a custom handle for the resource. This is typically done during resource initialization.
        *
        * @param value The handle to associate with the resource.
        */
        auto SetHandle( const Handle value ) -> void { m_Handle = value; }

        /**
        * @brief Sets the allocation status of the resource.
        *
        * This function updates whether the resource is considered allocated and
        * ready for use. It is typically used internally by the resource pool
        * when obtaining or releasing resources.
        *
        * @param value `true` to mark the resource as allocated, `false` to mark it as unavailable.
        */
        auto SetIsReady(const bool value) -> void { m_IsAllocated = value; }


        auto IsUsed() const -> bool { return GetRefCount() != 0; }

    protected:
        /**
        * @brief Allocates the resource.
        *
        * This method is responsible for allocating the resource within the resource pool.
        * Derived classes should implement the actual resource allocation logic, which may involve memory allocation or
        * system-level resource initialization. After a succesful call to this method, the resource is considered ready for use.
        *
        * @note This method should be implemented by derived classes to handle resource allocation.
        */
        virtual auto Initialize() -> void = 0;

        /**
        * @brief Releases the resource.
        *
        * This method is responsible for releasing a previously allocated resource. Derived classes should implement
        * the actual resource release logic, which may include freeing memory, closing file handles, or cleaning up other system resources.
        *
        * @note This method should be implemented by derived classes to handle resource deallocation.
        */
        virtual auto Release() -> void = 0;

    protected:
        bool m_IsAllocated{ false };

        Handle m_Handle{};

#if !defined(NDEBUG)
    public:
        static inline Size s_ResourceCount{ 0 };
#endif

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
        auto Init( const UInt32 poolSize ) -> void {
            m_PoolSize = poolSize;
            m_FreeHandles.reserve( poolSize );

            for ( Handle i{ 0 }; i < poolSize; ++i ) {
                m_FreeHandles.emplace_back( i );
            }

            m_Resources.reserve( poolSize );
        }

        /**
        * @brief Shuts down the resource pool, releasing all allocated resources.
        */
        auto Shutdown() -> void {
            m_FreeHandles.clear();
            m_Resources.clear();
        }

        auto Clear() -> void {

            m_FreeHandles.clear();
            m_Resources.clear();

            Init(m_PoolSize);
        }

    protected:
        /**
        * @brief Releases a resource back to the pool.
        * @param index The index of the resource to be released.
        */
        auto ReleaseResource( const Handle index ) -> void {

            if ( const auto it{ m_Resources.find(index) }; it != m_Resources.end() ) {
                it->second = ResourceHandle::CreateEmpty();
                m_FreeHandles.emplace_back(index);
            }
        }

        /**
        * @brief Accesses a resource by its index.
        * @param index The index of the resource to access.
        * @return A pointer to the resource, or nullptr if invalid.
        */
        MKT_NODISCARD auto AccessResource( const Handle index ) const -> ResourceHandle {
            return m_Resources.contains(index) ? m_Resources.at(index) : ResourceHandle::CreateEmpty();
        }

    protected:
        /**
        * @brief Gets a resource slot from the pool.
        * @return The index of the obtained resource, or `INVALID_HANDLE` if allocation fails.
        */
        MKT_NODISCARD auto ObtainResource() -> Handle {
            const Handle index{ m_FreeHandles.back() };
            m_FreeHandles.pop_back();

            return index;
        }

        auto Resize() -> void {
            // When pool is full free handles will be empty

            // Add the other handles after current limit size
            // Last available handle ID is m_PoolSize - 1 (see init)
            for (UInt32 i{}; i <  (m_PoolSize * POOL_RESIZE_RATE) - m_PoolSize; ++i ) {
                m_FreeHandles.emplace_back( i + m_PoolSize );
            }

            m_PoolSize = m_PoolSize + m_FreeHandles.size();
        }

        MKT_NODISCARD auto IsPoolFull() const -> bool { return m_FreeHandles.size() == 0; }

    protected:
        static constexpr double POOL_RESIZE_RATE{ 2.5 };

    protected:
        // TODO:
        //HeapAllocator* m_Allocator{ nullptr };

        UInt32 m_PoolSize{ 100 };
        std::vector<Handle> m_FreeHandles{};
        std::unordered_map<Handle, ResourceHandle> m_Resources{};
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

            const UInt32 index{ ObtainResource() };

            Pointer ptr{ new (std::nothrow) T(std::forward<Args>(args)... ) };
            ResourceHandle newResource{ ResourceHandle::Create(ptr) };

            m_Resources[index] = newResource;
            m_Resources[index]->SetHandle( index );

            return m_Resources[index];
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
            for (ResourceHandle& resource : m_Resources  | std::views::values ) {
                if (!resource.IsEmpty() && resource->IsReady()) {
                    result = resource.As<T>();
                    break;
                }
            }

            return result;
        }

        auto begin() { return m_Resources.begin(); }
        auto end() { return m_Resources.end(); }

        auto cbegin() const { return m_Resources.cbegin(); }
        auto cend() const { return m_Resources.cend(); }
    };

}

#endif// RESOURCEPOOL_HH
