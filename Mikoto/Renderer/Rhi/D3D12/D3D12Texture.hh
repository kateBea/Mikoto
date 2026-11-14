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

#ifndef MIKOTO_RENDERER_RHI_D3D12_D3D12_TEXTURE_HH
#define MIKOTO_RENDERER_RHI_D3D12_D3D12_TEXTURE_HH

#include <mutex>

#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Texture.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <Platform/Windows/Windows.hh>

#include <directx/d3d12.h>
#include <directx/d3dx12.h>

#include <Renderer/Rhi/D3D12/Direct3D12Helpers.hh>
#include <Renderer/Rhi/D3D12/D3D12MemoryAllocator.hh>

namespace mikoto::renderer::d3d12 {

    class DeviceResources;

    class Sampler final : public rhi::ISampler {
    public:
        auto GetMinFilter() const -> rhi::SamplerFilter override;
        auto GetMagFilter() const -> rhi::SamplerFilter override;
        auto GetWrapU() const -> rhi::SamplerWrapMode override;
        auto GetWrapV() const -> rhi::SamplerWrapMode override;
        auto GetWrapW() const -> rhi::SamplerWrapMode override;
        auto GetMipLevels() const -> float override;

    private:
        float mMipLevels{ 1.0f };

        rhi::SamplerFilter mMinFilter{ rhi::SamplerFilter::eNearest };
        rhi::SamplerFilter mMagFilter{ rhi::SamplerFilter::eNearest };
        rhi::SamplerWrapMode mWrapU{ rhi::SamplerWrapMode::eRepeat };
        rhi::SamplerWrapMode mWrapV{ rhi::SamplerWrapMode::eRepeat };
        rhi::SamplerWrapMode mWrapW{ rhi::SamplerWrapMode::eRepeat };

        rhi::Color mBorderColor{};

    public:
        explicit Sampler( const rhi::SamplerCreateDescription& desc, DeviceResources& resources );

        MKT_NODISCARD auto GetSamplerIndex() const -> DescriptorIndex;

        auto AllocateSampler(D3D12_CPU_DESCRIPTOR_HANDLE handle) -> void;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        ~Sampler() override;

    private:
        auto Destroy() -> void override;
        auto Initialize() -> void override;

    public:
        auto SetDebugName( eastl::string_view name ) -> void override;

    private:
        D3D12_SAMPLER_DESC mSamplerDescription{};

        // For shader model 6.6
        DescriptorIndex mSamplerResourceIndex{ kInvalidDescriptorIndex };
    };

    struct ExternalTextureDescription {
        core::u32 mWidth{};
        core::u32 mHeight{};
        rhi::Format mFormat{ rhi::Format::eBGRA8_UNORM };

        rhi::TextureUsageFlags mTextureUsage{};

        Microsoft::WRL::ComPtr<ID3D12Resource> mImageResource{};
        rhi::DeviceObjectHandle mStorageOwner{};
        core::u32 mPlaneSlice{};
    };

    class Texture : public rhi::ITexture {
    public:
        explicit Texture( const ExternalTextureDescription& spec, DeviceResources& resources );
        explicit Texture( const rhi::TextureCreateDescription& desc, DeviceResources& resources );

        auto SetDebugName( eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        MKT_NODISCARD auto GetResourceIndex() const -> DescriptorIndex;

        auto GetStorageSizeBytes() const -> core::usize override;

        auto GetFormat() const -> rhi::Format override;

        auto GetWidth() const -> core::u32 override;
        auto GetHeight() const -> core::u32 override;

        // Maybe move to assets these are asset properties
        auto GetImage() const -> asset::ImageHandle override;
        auto GetTextureUsage() const -> rhi::TextureUsageFlags override;

        auto GetSizeBytes() const -> core::usize override;
        auto GetDimension() const -> rhi::TextureDimension override;

        auto GetSampleCount() const -> rhi::Multisampling override;
        auto GetMipLevelCount() const -> core::u32 override;

        auto CreateSrv(core::usize descriptor, rhi::TextureSubresourceSet subResources, rhi::Format format = rhi::Format::eUnknown, rhi::TextureDimension dimension = rhi::TextureDimension::eInvalid) const -> void;
        auto CreateUav(core::usize descriptor, rhi::TextureSubresourceSet subResources, rhi::Format format = rhi::Format::eUnknown, rhi::TextureDimension dimension = rhi::TextureDimension::eInvalid) const -> void;

        auto CreateRtv(core::usize descriptor, rhi::TextureSubresourceSet subResources, rhi::Format format = rhi::Format::eUnknown ) const -> void;
        auto CreateDsv(core::usize descriptor, rhi::TextureSubresourceSet subResources, bool isReadOnly = false) const -> void;

        MKT_NODISCARD operator ID3D12Resource*() const;

        MKT_NODISCARD auto GetRtvDescriptorIndex() const -> DescriptorIndex;
        MKT_NODISCARD auto GetDsvDescriptorIndex() const -> DescriptorIndex;

        MKT_NODISCARD auto GetRtvDescriptorIndex( rhi::TextureSubresourceSet subresources ) -> DescriptorIndex;
        MKT_NODISCARD auto GetDsvDescriptorIndex( rhi::TextureSubresourceSet subresources, bool readOnly = false ) -> DescriptorIndex;

        ~Texture() override;

    private:
        struct AttachmentView {
            rhi::TextureSubresourceSet mSubResources{};
            DescriptorIndex mIndex{ kInvalidDescriptorIndex };
            bool mReadOnly{};
        };

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

        auto InitInitialData2D( memory::BufferSpanHandle buffer ) -> void;
        auto InitInitialDataCube( memory::BufferSpanHandle buffer ) -> void;

        MKT_NODISCARD auto NormalizeSubResources( rhi::TextureSubresourceSet subResources ) const -> rhi::TextureSubresourceSet;

    private:
        core::u32 mWidth{};
        core::u32 mHeight{};
        core::u32 mMipCount{ 1 };

        asset::ImageHandle mImageData{};
        memory::BufferSpanHandle mBufferSpan{};

        rhi::Format mFormat{ rhi::Format::eUnknown };
        rhi::TextureDimension mDimension{ rhi::TextureDimension::eInvalid };

        const rhi::ResourceStates mInitialState;

        rhi::TextureUsageFlags mTextureUsage{ rhi::TextureUsageFlagsBits::ShaderResource };

        rhi::Multisampling mMultisampling{ rhi::Multisampling::eMsaaX1 };

        rhi::TextureSubresourceSet mSubResources{};

        // Keep external memory alive until the native resource reference is released.
        rhi::DeviceObjectHandle mExternalStorageOwner{};
        ImageAllocation mImageAllocation{};
        bool mKeepInitializerResources{ false };

        DeviceResources* mResources{};

        bool mIsExternalImage{};
        core::u32 mPlaneSlice{};

        D3D12_CLEAR_VALUE mOptimizedClearValue{};

        DescriptorIndex mRtvDescriptorIndex{ kInvalidDescriptorIndex };
        DescriptorIndex mDsvDescriptorIndex{ kInvalidDescriptorIndex };

        std::mutex mViewMutex{};
        eastl::vector<AttachmentView> mRenderTargetViews{};
        eastl::vector<AttachmentView> mDepthStencilViews{};

        // For shader model 6.6
        DescriptorIndex mShaderResourceIndex{ kInvalidDescriptorIndex };
    };

}// namespace mikoto::renderer::d3d12

#endif

#endif// MIKOTO_RENDERER_RHI_D3D12_D3D12_TEXTURE_HH

