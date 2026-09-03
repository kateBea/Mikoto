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

    struct Object {
        enum class Type {
            Pointer,
            Integer,
            None
        };

        Type mType{ Type::None };
        void* mPointer{ nullptr };
        core::u64 mInteger{ 0 };

        explicit Object(void* p) : mType(Type::Pointer), mPointer(p) {}
        explicit Object(core::u64 i) : mType(Type::Integer), mInteger(i) {}
        Object() = default;

        template<typename T>
        operator T*() const {
            if (mType == Type::Pointer) {
                return core::as<T*>(mPointer);
            }

            return nullptr;
        }
    };

    class DeviceObject : public core::IResource {
    public:
        explicit DeviceObject() = default;

        auto Initialize( IGpuDevice* device ) -> void;

        auto SetResourceState( ResourceStates state ) -> void;
        MKT_NODISCARD auto GetResourceState() const -> ResourceStates;

        virtual auto SetDebugName( eastl::string_view name ) -> void;

        MKT_NODISCARD auto GetDebugName() const -> eastl::string_view;
        MKT_NODISCARD static auto GetDefaultDebugName() -> eastl::string_view;

        MKT_NODISCARD virtual auto GetNativeHandle( ObjectType ) -> Object;
        MKT_NODISCARD virtual auto GetNativeHandle( ObjectType type ) const -> Object;

        MKT_NODISCARD auto GetHeapType() const -> HeapType { return mHeapType; }

        ~DeviceObject() override = default;

    protected:
        DeviceObject( HeapType heapType, ResourceType resourceType );

        auto Initialize() -> void override = 0;
        auto Release() -> void override = 0;

    protected:
        IGpuDevice* mDevice{};
        eastl::string mDebugName{};

        // State tracking
        ResourceType mResourceType{ ResourceType::eInvalid };
        ResourceStates mResourceState{ ResourceStates::eUnknown };

        AccessFlags mAccessAfter{ AccessFlagsBits::kNone };
        TextureLayoutFlags mLayoutAfter{ TextureLayoutBits::kUnknown };
        PipelineStageFlags mStageAfter{ PipelineStageFlagsBits::kNone };

        // By default, the resource is device local
        // lives in memory "only accessible by device"
        HeapType mHeapType{ HeapType::eDeviceLocal };
        CpuAccessType mCpuAccess{ CpuAccessType::eNone };
    };

    using DeviceObjectHandle = core::Ref<DeviceObject>;
}// namespace mikoto::renderer::rhi

#endif //MIKOTO_RHI_DEVICE_OBJECT_HH
