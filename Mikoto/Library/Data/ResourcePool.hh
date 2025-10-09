//
// Created by zanet on 3/27/2025.
//

#ifndef RESOURCEPOOL_HH
#define RESOURCEPOOL_HH
#include <ranges>
#include <type_traits>

#include <ankerl/unordered_dense.h>

#include <Common/Common.hh>
#include <Logging/Assert.hh>
#include <Library/Utility/Types.hh>
#include <Common/ReferenceCounted.hh>

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
        /**
        * @brief Virtual destructor.
        *
        * A virtual destructor to ensure proper cleanup of derived class objects when deleted through a base pointer.
        */
        ~IResource() override = default;

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
        virtual auto Allocate() -> void = 0;

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
    };

    /**
    * @class ResourcePool
    * @brief Manages a pool of reusable resources [Internal].
    *
    * The `ResourcePool` class provides an efficient way to manage resources,
    * allowing reuse and minimizing allocations. It is designed to store and
    * retrieve resources dynamically as needed. `IResource`'s registered to a resource pool
    * do need to be free-d by the user, the pool does not take care of deallocating resources
    */
    class ResourcePool {
    public:
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
            for ( const auto& resource: m_Resources | std::views::values ) {
                if ( !resource.IsEmpty() ) {
                    // Check if resource was not freed
                    ReleaseResource( resource->GetHandle() );
                }
            }

            m_FreeHandles.clear();
            m_Resources.clear();
        }

    protected:
        /**
        * @brief Releases a resource back to the pool.
        * @param index The index of the resource to be released.
        */
        auto ReleaseResource( const Handle index ) -> void {

            if ( m_Resources.contains(index) ) {
                m_Resources[index]->Free();
                m_Resources[index] = nullptr;

                m_FreeHandles.emplace_back(index);
            }
        }

        /**
        * @brief Accesses a resource by its index.
        * @param index The index of the resource to access.
        * @return A pointer to the resource, or nullptr if invalid.
        */
        MKT_NODISCARD auto AccessResource( const Handle index ) const -> Ref<IResource> {
            return m_Resources.contains(index) ? m_Resources.at(index) : Ref<IResource>::CreateEmpty();
        }

    protected:
        /**
        * @brief Gets a resource slot from the pool.
        * @return The index of the obtained resource, or `INVALID_HANDLE` if allocation fails.
        */
        MKT_NODISCARD auto ObtainResource() -> Handle {
            if ( !IsHandleAvailable() ) {
                return INVALID_HANDLE;
            }

            const Handle index{ m_FreeHandles.back() };

            // TODO: avoid adding/removing elements from this list
            m_FreeHandles.pop_back();

            return index;
        }

        /**
        * @brief Checks if a handle is available for use.
        * @return True if the handle is available, false otherwise.
        *
        * This function determines whether we have handles free for use.
        */
        MKT_NODISCARD auto IsHandleAvailable() const -> bool {
            return !m_FreeHandles.empty();
        }

    protected:

    protected:
        UInt32 m_PoolSize{ 100 };
        UInt32 m_IndexFreeHandles{};
        std::vector<Handle> m_FreeHandles{};
        ankerl::unordered_dense::map<Handle, Ref<IResource>> m_Resources{};
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
            const UInt32 index{ ObtainResource() };
            if (index == INVALID_HANDLE) {
                return RefHandle::CreateEmpty();
            }

            Pointer p{ new (std::nothrow) T(std::forward<Args>(args)... ) };
            auto newResource{ std::move(Ref<IResource>::Create(p)) };

            const auto [it, success]{ m_Resources.try_emplace( index, std::move(newResource) ) };
            if (success) {
                p->SetHandle( index );
            } else if (it->second.IsEmpty()) {
                it->second = Ref<IResource>::Create(p);
            }

            return it->second.As<T>();
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

        auto begin() { return m_Resources.begin(); }
        auto end() { return m_Resources.end(); }

        auto cbegin() const { return m_Resources.cbegin(); }
        auto cend() const { return m_Resources.cend(); }
    };

}

#endif// RESOURCEPOOL_HH
