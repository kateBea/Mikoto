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

#ifndef MIKOTO_RHI_DEVICE_OBJECT_HH
#define MIKOTO_RHI_DEVICE_OBJECT_HH

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/ResourcePool.hh>

#include <Renderer/Rhi/Types.hh>

namespace mikoto::renderer::rhi {

    class IGpuDevice;

    /**
     * Type-erased native backend handle returned by an RHI resource.
     */
    struct Object {
        enum class Type {
            Pointer,
            Integer,
            None
        };

        Type mType{ Type::None };
        void* mPointer{ nullptr };
        core::u64 mInteger{ 0 };

        /**
         * Wraps a native pointer handle.
         *
         * @param p Native pointer handle.
         */
        explicit Object(void* p);

        /**
         * Wraps a native integer handle.
         *
         * @param i Native integer handle.
         */
        explicit Object(core::u64 i);
        Object() = default;

        /**
         * Converts a pointer-native handle to @p T.
         *
         * @returns The typed pointer, or null when this object holds an integer handle.
         * @tparam T pointed-to native backend type.
         */
        template<typename T>
        operator T*() const {
            if (mType == Type::Pointer) {
                return core::as<T*>(mPointer);
            }

            return nullptr;
        }
    };

    /**
     * Common base for RHI resources owned by an @ref IGpuDevice.
     *
     * It centralizes device ownership, debug names, native-handle access and
     * optional resource-state tracking for backend implementations.
     */
    class DeviceObject : public core::IResource {
    public:

        /**
         * Creates an uninitialized device object.
         */
        explicit DeviceObject() = default;

        /**
         * Associates the object with a device and initializes its backend resource.
         * @param device Input value used by this operation.
         */
        auto Initialize( IGpuDevice* device ) -> void;

        /**
         * Updates the tracked resource state.
         * @param state Input value used by this operation.
         */
        auto SetResourceState( ResourceStates state ) -> void;

        /**
         * Returns the last tracked resource state.
         * @returns The result of GetResourceState.
         */
        MKT_NODISCARD auto GetResourceState() const -> ResourceStates;

        /**
         * Returns the revision of the recorded resource state.
         * @returns A counter advanced by every SetResourceState call, including same-state updates.
         */
        MKT_NODISCARD auto GetResourceStateRevision() const -> core::u64;

        /**
         * Sets the human-readable debug label for this resource.
         * @param name Input value used by this operation.
         */
        virtual auto SetDebugName( eastl::string_view name ) -> void;

        /**
         * Returns the debug label assigned to this resource.
         * @returns The result of GetDebugName.
         */
        MKT_NODISCARD auto GetDebugName() const -> eastl::string_view;

        /**
         * Returns the fallback debug label for this resource type.
         * @returns The result of GetDefaultDebugName.
         */
        MKT_NODISCARD static auto GetDefaultDebugName() -> eastl::string_view;

        /**
         * Returns a mutable native backend handle of the requested type.
         * @param type Requested native-object type.
         * @returns The result of GetNativeHandle.
         */
        MKT_NODISCARD virtual auto GetNativeHandle( ObjectType type ) -> Object;

        /**
         * Returns a native backend handle without mutating this resource.
         * @param type Input value used by this operation.
         * @returns The result of GetNativeHandle.
         */
        MKT_NODISCARD virtual auto GetNativeHandle( ObjectType type ) const -> Object;

        /**
         * Returns the memory heap selected for this resource.
         * @returns The result of GetHeapType.
         */
        MKT_NODISCARD auto GetHeapType() const -> HeapType;

        /**
         * Destroys the device object.
         */
        ~DeviceObject() override = default;

    protected:

        /**
         * Constructs a device object with its allocation and resource classes.
         * @param heapType Heap class used by the object.
         * @param resourceType Resource class represented by the object.
         */
        DeviceObject( HeapType heapType, ResourceType resourceType );

        /**
         * Creates the backend resource after its device has been assigned.
         */
        auto Initialize() -> void override = 0;

        /**
         * Releases the native backend resource owned by this object.
         */
        auto Destroy() -> void override = 0;

    protected:
        IGpuDevice* mDevice{};
        eastl::string mDebugName{};

        // State tracking
        ResourceType mResourceType{ ResourceType::eInvalid };
        ResourceStates mResourceState{ ResourceStates::eUnknown };
        core::u64 mResourceStateRevision{};

        // By default, the resource is device local
        // lives in memory "only accessible by device"
        HeapType mHeapType{ HeapType::eDeviceLocal };
        AccessType mCpuAccess{ AccessType::eNone };
    };

    using DeviceObjectHandle = core::Ref<DeviceObject>;
}// namespace mikoto::renderer::rhi

#endif //MIKOTO_RHI_DEVICE_OBJECT_HH
