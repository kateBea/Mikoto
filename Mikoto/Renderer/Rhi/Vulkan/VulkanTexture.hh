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

#ifndef MIKOTO_RENDERER_RHI_VULKAN_VULKAN_TEXTURE_HH
#define MIKOTO_RENDERER_RHI_VULKAN_VULKAN_TEXTURE_HH

#include <volk.h>

#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Vulkan/VulkanMemoryAllocator.hh>

namespace mikoto::renderer::vulkan {

    class Sampler final : public rhi::ISampler {
    public:
        MKT_NODISCARD auto GetMinFilter() const -> rhi::SamplerFilter override;
        MKT_NODISCARD auto GetMagFilter() const -> rhi::SamplerFilter override;
        MKT_NODISCARD auto GetWrapU() const -> rhi::SamplerWrapMode override;
        MKT_NODISCARD auto GetWrapV() const -> rhi::SamplerWrapMode override;
        MKT_NODISCARD auto GetWrapW() const -> rhi::SamplerWrapMode override;
        MKT_NODISCARD auto GetMipLevels() const -> float override;

    private:
        float mMipLevels{ 1.0f };

        rhi::SamplerFilter mMinFilter{ rhi::SamplerFilter::eNearest };
        rhi::SamplerFilter mMagFilter{ rhi::SamplerFilter::eNearest };
        rhi::SamplerWrapMode mWrapU{ rhi::SamplerWrapMode::eRepeat };
        rhi::SamplerWrapMode mWrapV{ rhi::SamplerWrapMode::eRepeat };
        rhi::SamplerWrapMode mWrapW{ rhi::SamplerWrapMode::eRepeat };

        rhi::Color mBorderColor{};

    public:
        explicit Sampler( const rhi::SamplerCreateDescription &desc );

        auto SetDebugName( eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        MKT_NODISCARD operator VkSampler() const;

        ~Sampler() override;

    private:
        auto Destroy() -> void override;
        auto Initialize() -> void override;

    private:
        VkSampler mSampler{ VK_NULL_HANDLE };
        VkSamplerCreateInfo mCreateInfo{};
    };

    struct ExternalTextureDescription {
        core::u32 mWidth{};
        core::u32 mHeight{};

        rhi::Format mSurfaceFormat{};
        VkImageViewCreateInfo mImageViewCreateInfo{};
        rhi::TextureUsageFlags mUsage{ rhi::TextureUsageFlagsBits::RenderTarget | rhi::TextureUsageFlagsBits::CopyDest };
        core::Ref<core::Resource> mOwner{};
    };

    class Texture final : public rhi::ITexture {
    public:
        explicit Texture( const rhi::TextureCreateDescription &data );
        explicit Texture( const ExternalTextureDescription &spec );

        auto SetDebugName( eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetSizeBytes() const -> core::usize override;
        MKT_NODISCARD auto GetStorageSizeBytes() const -> core::usize override;

        MKT_NODISCARD auto GetFormat() const -> rhi::Format override;
        MKT_NODISCARD auto GetWidth() const -> core::u32 override;
        MKT_NODISCARD auto GetHeight() const -> core::u32 override;
        MKT_NODISCARD auto GetDepth() const -> core::u32 override;

        MKT_NODISCARD auto GetTextureUsage() const -> rhi::TextureUsageFlags override;

        MKT_NODISCARD auto GetImage() const -> asset::ImageHandle override;


        MKT_NODISCARD auto GetDimension() const -> rhi::TextureDimension override;
        MKT_NODISCARD auto GetSampleCount() const -> rhi::Multisampling override;
        MKT_NODISCARD auto GetMipLevelCount() const -> core::u32 override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        MKT_NODISCARD auto GetAspectMask() const -> VkImageAspectFlags;

        MKT_NODISCARD auto GetRenderView( core::u32 mipLevel, core::u32 face = 0 ) const -> const VkImageView &;

        MKT_NODISCARD operator VkImage() const;

        ~Texture() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

        auto CopyInitialData( memory::BufferSpanHandle buffer ) -> void;

    private:
        core::u32 mWidth{};
        core::u32 mHeight{};
        core::u32 mDepth{ 1 };
        core::u32 mMipCount{ 1 };

        asset::ImageHandle mImageData{};
        memory::BufferSpanHandle mBufferSpan{};

        rhi::Format mFormat{ rhi::Format::eUnknown };
        rhi::TextureDimension mDimension{ rhi::TextureDimension::eInvalid };

        const rhi::ResourceStates mInitialState;

        rhi::TextureSubresourceSet mSubResources{};
        rhi::Multisampling mMultisampling{ rhi::Multisampling::eMsaaX1 };
        rhi::TextureUsageFlags mTextureUsage{ rhi::TextureUsageFlagsBits::ShaderResource };

        VkImageAspectFlags mAspectFlags{};
        ImageAllocation mImageAllocation{};

        // For every face we keep a list of mips
        // For every mip we keep a view
        // This account for stuff like IBL Prefilter map where you have
        // multiple mips per face
        eastl::vector<eastl::vector<VkImageView>> mImageViewRtvList{};
        VkImageView mImageViewSrv{};

        VkImageViewCreateInfo mImageViewCreateInfo{};

        bool mIsImageExternal{ false };
        bool mKeepInitializerResources{ false };
        core::Ref<core::Resource> mExternalOwner{};
    };
}// namespace mikoto::renderer::vulkan

#endif// MIKOTO_RENDERER_RHI_VULKAN_VULKAN_TEXTURE_HH
