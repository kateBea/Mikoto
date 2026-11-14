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

#ifndef MIKOTO_RENDERER_RHI_D3D12_D3D12_BUFFER_HH
#define MIKOTO_RENDERER_RHI_D3D12_D3D12_BUFFER_HH

#include <Core/Core.hh>
#include <Core/Platform.hh>
#include <Core/Types.hh>
#include <Renderer/Rhi/Buffer.hh>
#include <Renderer/Rhi/Types.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

// D3D12 extension library.
#include <Platform/Windows/Windows.hh>

#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <wrl.h>

#include <Renderer/Rhi/D3D12/D3D12MemoryAllocator.hh>
#include <Renderer/Rhi/D3D12/Direct3D12Helpers.hh>

namespace mikoto::renderer::d3d12 {

    class DeviceResources;

    class Buffer final : public rhi::IBuffer {
    public:
        auto GetUsage() const -> rhi::BufferUsageFlags override;
        auto GetDataType() const -> rhi::BufferDataType override;
        auto GetData() const -> memory::BufferSpanHandle override;
        auto GetSizeBytes() const -> core::usize override;
        auto GetFormat() const -> rhi::Format override;
        auto GetCount() const -> core::usize override;

    private:
        memory::BufferSpanHandle mUploadContents{};

        // These 2 attributes specify that the buffer is typed
        // meaning it will hold mElementCount elements of mElementSize size in bytes
        // This is useful for the backend API to manage alignment as it considers necessary
        // If it is going to be treated as raw bytes
        // then mElementSize holds the total size in bytes
        core::usize mElementCount{};
        core::usize mElementSize{};

        rhi::BufferDataType mDataType{ rhi::BufferDataType::eInvalid };
        rhi::BufferUsageFlags mUsage{ rhi::BufferUsageFlagsBits::None };

        const rhi::ResourceStates mInitialState;

        bool mIsVolatile{};
        core::usize mMaxVersions{ 0 };

        rhi::Format mFormat{ rhi::Format::eUnknown };

    public:
        explicit Buffer( const rhi::BufferCreateDescription& createInfo, DeviceResources& resources );

        auto PersistentMap() -> void*;
        auto PersistentUnmap() -> void;

        auto SetDebugName( eastl::string_view name) -> void override;

        MKT_NODISCARD auto GetGpuDeviceAddress() -> rhi::DeviceAddress override;
        MKT_NODISCARD auto GetShaderDeviceAddress() -> rhi::DeviceAddress override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        MKT_NODISCARD auto IsMapped() const -> bool;
        MKT_NODISCARD auto GetMappedAddress() -> void*;
        MKT_NODISCARD auto GetMappedAddress() const -> const void*;

        MKT_NODISCARD auto GetResourceIndex() const -> DescriptorIndex;


        auto CreateCBV(core::usize descriptor, rhi::BufferRange range, rhi::Format format = rhi::Format::eUnknown) const -> void;
        auto CreateSRV(core::usize descriptor, rhi::BufferRange range, rhi::ResourceType resourceType = rhi::ResourceType::eInvalid, rhi::Format format = rhi::Format::eUnknown) const -> void;
        auto CreateUAV(core::usize descriptor, rhi::BufferRange range, rhi::ResourceType resourceType = rhi::ResourceType::eInvalid, rhi::Format format = rhi::Format::eUnknown) const -> void;

        static auto CreateNullSRV(core::usize descriptor, rhi::Format format, ID3D12Device2* device) -> void;
        static auto CreateNullUAV(core::usize descriptor, rhi::Format format, ID3D12Device2* device) -> void;

        MKT_NODISCARD operator ID3D12Resource*() const;

        ~Buffer() override;

    private:
        auto Destroy() -> void override;
        auto Initialize() -> void override;

    private:
        void* mMappedAddress{};
        BufferAllocation mAllocation{};
        bool mKeepInitializerResources{ false };

        DeviceResources* mResources{};

        DescriptorIndex mSrvDescriptorIndex{ kInvalidDescriptorIndex };

        // For shader model 6.6
        DescriptorIndex mShaderResourceIndex{ kInvalidDescriptorIndex };
    };
}

#endif

#endif// MIKOTO_RENDERER_RHI_D3D12_D3D12_BUFFER_HH

