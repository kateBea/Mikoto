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

#include <Renderer/Rhi/DeviceObject.hh>

namespace mikoto::renderer::rhi {

    DeviceObject::DeviceObject( HeapType heapType, ResourceType resourceType )
       : mResourceType{ resourceType }, mHeapType{ heapType }
    {}

    auto DeviceObject::Initialize( IGpuDevice *device ) -> void {
        mDevice = device;
        Initialize();
    }

    auto DeviceObject::SetResourceState( ResourceStates state ) -> void {
        mResourceState = state;
    }

    auto DeviceObject::GetResourceState() const -> ResourceStates {
        return mResourceState;
    }

    auto DeviceObject::SetDebugName( eastl::string_view name ) -> void {
        mDebugName = name;
    }

    auto DeviceObject::GetDebugName() const -> eastl::string_view {
        return mDebugName;
    }

    auto DeviceObject::GetDefaultDebugName() -> eastl::string_view {
        return "DeviceObject";
    }

    auto DeviceObject::GetNativeHandle( ObjectType ) -> Object {
        return Object(nullptr);
    }

    auto DeviceObject::GetNativeHandle( ObjectType type ) const -> Object {
        return const_cast<DeviceObject*>(this)->GetNativeHandle( type );
    }
}// namespace mikoto::renderer::rhi
