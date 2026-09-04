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

#ifndef MIKOTO_D3D12_TEXTURE_HH
#define MIKOTO_D3D12_TEXTURE_HH

#include <Core/Platform.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Texture.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <directx/d3d12.h>
#include <directx/d3dx12.h>

#include <Renderer/Rhi/D3D12/Direct3D12Helpers.hh>
#include <Renderer/Rhi/D3D12/D3D12MemoryAllocator.hh>

namespace mikoto::renderer::d3d12 {

    class DeviceResources;

    class Sampler final : public rhi::ISampler {
    public:
        explicit Sampler( const rhi::SamplerCreateDescription& desc, DeviceResources& resources );

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        ~Sampler() override;

    private:
        auto Release() -> void override;
        auto Initialize() -> void override;

    public:
        auto SetDebugName( eastl::string_view name ) -> void override;

    private:
        DeviceResources* mDeviceResources{};
        DescriptorIndex mSamplerDescriptorIndex{ kInvalidDescriptorIndex };

        D3D12_SAMPLER_DESC mSamplerDescription{};
    };

    struct ExternalTextureDescription {
        core::u32 mWidth{};
        core::u32 mHeight{};
        rhi::Format mFormat{ rhi::Format::eBGRA8_UNORM };

        rhi::TextureUsageFlags mTextureUsage{};

        Microsoft::WRL::ComPtr<ID3D12Resource> mImageResource{};
    };

    class Texture : public rhi::ITexture {
    public:
        explicit Texture( const ExternalTextureDescription& spec, DeviceResources& resources );
        explicit Texture( const rhi::TextureCreateDescription& desc, DeviceResources& resources );

        auto SetDebugName( eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        auto CreateSRV(SIZE_T descriptor, rhi::TextureSubresourceSet subResources, rhi::Format format = rhi::Format::eUnknown, rhi::TextureDimension dimension = rhi::TextureDimension::eInvalid) const -> void;
        auto CreateUAV(SIZE_T descriptor, rhi::TextureSubresourceSet subResources, rhi::Format format = rhi::Format::eUnknown, rhi::TextureDimension dimension = rhi::TextureDimension::eInvalid) const -> void;

        auto CreateRTV(SIZE_T descriptor, rhi::TextureSubresourceSet subResources, rhi::Format format = rhi::Format::eUnknown ) const -> void;
        auto CreateDSV(SIZE_T descriptor, rhi::TextureSubresourceSet subResources, bool isReadOnly = false) const -> void;

        MKT_NODISCARD operator ID3D12Resource*() const;

        MKT_NODISCARD auto GetRtvDescriptorIndex() const -> DescriptorIndex;
        MKT_NODISCARD auto GetDsvDescriptorIndex() const -> DescriptorIndex;

        ~Texture() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

        auto InitInitialData2D( memory::BufferSpanHandle buffer ) -> void;
        auto InitInitialDataCube( memory::BufferSpanHandle buffer ) -> void;

    private:
        ImageAllocation mImageAllocation{};
        bool mKeepInitializerResources{ false };

        DeviceResources* mResources{};

        bool mIsExternalImage{};

        D3D12_CLEAR_VALUE mOptimizedClearValue{};

        DescriptorIndex mRtvDescriptorIndex{ kInvalidDescriptorIndex };
        DescriptorIndex mDsvDescriptorIndex{ kInvalidDescriptorIndex };
    };

}// namespace mikoto::renderer::d3d12

#endif

#endif//MIKOTO_D3D12_TEXTURE_HH
