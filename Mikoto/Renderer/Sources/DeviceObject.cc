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

#include <Core/Profiler.hh>

#include <Renderer/Rhi/DeviceObject.hh>

namespace mikoto::renderer::rhi {

    Object::Object( void* pointer )
        : mType{ Type::Pointer }, mPointer{ pointer } {
    }

    Object::Object( core::u64 integer )
        : mType{ Type::Integer }, mInteger{ integer } {
    }

    DeviceObject::DeviceObject( HeapType heapType, ResourceType resourceType )
        : mResourceType{ resourceType }, mHeapType{ heapType } {
    }

    auto DeviceObject::Initialize( IGpuDevice* device ) -> void {
        MKT_BEGIN_PROFILER();
        mDevice = device;
        Initialize();
    }

    auto DeviceObject::SetResourceState( ResourceStates state ) -> void {
        MKT_BEGIN_PROFILER();
        mResourceState = state;
    }

    auto DeviceObject::GetResourceState() const -> ResourceStates {
        MKT_BEGIN_PROFILER();
        return mResourceState;
    }

    auto DeviceObject::SetDebugName( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();
        mDebugName = name;
    }

    auto DeviceObject::GetDebugName() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return mDebugName;
    }

    auto DeviceObject::GetDefaultDebugName() -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return "DeviceObject";
    }

    auto DeviceObject::GetNativeHandle( ObjectType ) -> Object {
        MKT_BEGIN_PROFILER();
        return Object( nullptr );
    }

    auto DeviceObject::GetNativeHandle( ObjectType type ) const -> Object {
        MKT_BEGIN_PROFILER();
        return const_cast<DeviceObject*>( this )->GetNativeHandle( type );
    }

    auto DeviceObject::GetHeapType() const -> HeapType {
        MKT_BEGIN_PROFILER();
        return mHeapType;
    }

    auto DeviceObject::GetOwningDevice() const -> IGpuDevice* {
        return mDevice;
    }
}
