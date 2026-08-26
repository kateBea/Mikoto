//    Copyright 2025 ケイト
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

#ifndef MIKOTO_D3D12BUFFER_HH
#define MIKOTO_D3D12BUFFER_HH

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Buffer.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

// D3D12 extension library.
#include <directx/d3d12.h>
#include <directx/d3dx12.h>

#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <wrl.h>

#include <Renderer/Rhi/D3D12/Direct3D12Helpers.hh>
#include <Renderer/Rhi/D3D12/D3D12MemoryAllocator.hh>

namespace mikoto::renderer::d3d12 {

    class DeviceResources;

    class Buffer final : public rhi::IBuffer {
    public:
        explicit Buffer( const rhi::BufferCreateDescription& createInfo, DeviceResources& resources );

        auto PersistentMap() -> void*;
        auto PersistentUnmap() -> void;

        auto SetDebugName( eastl::string_view name) -> void override;

        MKT_NODISCARD auto GetGpuDeviceAddress(IBuffer* buffer) -> rhi::DeviceAddress override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        MKT_NODISCARD auto IsMapped() const -> bool;
        MKT_NODISCARD auto GetMappedAddress() -> void*;
        MKT_NODISCARD auto GetMappedAddress() const -> const void*;

        auto CreateCBV(SIZE_T descriptor, rhi::BufferRange range, rhi::Format format = rhi::Format::eUnknown) const -> void;
        auto CreateSRV(SIZE_T descriptor, rhi::BufferRange range, rhi::ResourceType resourceType = rhi::ResourceType::eInvalid, rhi::Format format = rhi::Format::eUnknown) const -> void;
        auto CreateUAV(SIZE_T descriptor, rhi::BufferRange range, rhi::ResourceType resourceType = rhi::ResourceType::eInvalid, rhi::Format format = rhi::Format::eUnknown) const -> void;

        static auto CreateNullSRV(SIZE_T descriptor, rhi::Format format, ID3D12Device2* device) -> void;
        static auto CreateNullUAV(SIZE_T descriptor, rhi::Format format, ID3D12Device2* device) -> void;

        MKT_NODISCARD operator ID3D12Resource*() const;

        ~Buffer() override;

    private:
        auto Release() -> void override;
        auto Initialize() -> void override;

    private:
        void* mMappedAddress{};
        BufferAllocation mAllocation{};
        bool mKeepInitializerResources{ false };

        DeviceResources* mResources{};

        DescriptorIndex mSrvDescriptorIndex{ kInvalidDescriptorIndex };
    };
}

#endif

#endif//MIKOTO_D3D12BUFFER_HH
