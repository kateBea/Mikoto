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

#ifndef MIKOTO_VULKAN_TEXTURE2D_HH
#define MIKOTO_VULKAN_TEXTURE2D_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/vector.h>
#include <volk.h>

#include <Assets/Image.hh>
#include <Core/Core.hh>
#include <Core/String.hh>
#include <Core/Types.hh>

#include <Renderer/Core/Rhi.hh>

#include <Renderer/Vulkan/VulkanMemoryAllocator.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::asset;

    class Sampler final : public ISampler {
    public:
        explicit Sampler( const SamplerCreateDescription& desc );

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;

        ~Sampler() override;

    private:
        auto Release() -> void override;
        auto Initialize() -> void override;

    private:
        VkSampler mSampler{ VK_NULL_HANDLE };
        VkSamplerCreateInfo mCreateInfo{};
    };

    struct ExternalTextureDescription {
        u32 mWidth{};
        u32 mHeight{};

        Format mSurfaceFormat{};
        VkImageViewCreateInfo mImageViewCreateInfo{};
    };

    class Texture final : public ITexture {
    public:
        explicit Texture( const TextureCreateDescription& data );
        explicit Texture( const ExternalTextureDescription& spec );

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        MKT_NODISCARD auto HasExternalImage() const -> bool;

        MKT_NODISCARD auto GetAspectMask() const -> VkImageAspectFlags;

        MKT_NODISCARD auto GetView( u32 mipLevel, u32 face = 0 ) const -> const VkImageView&;

        auto SetDebugName( eastl::string_view name ) -> void override;

        MKT_NODISCARD auto IsSwapChainImage() const -> bool;

        ~Texture() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

        auto InitInitialData2D() -> void;
        auto InitInitialDataCube() -> void;

    private:
        VkImageAspectFlags mAspectFlags{};
        ImageAllocation mImageAllocation{};

        // For every face we keep a list of mips
        // For every mip we keep a view
        // This account for stuff like IBL Prefilter map where you have
        // multiple mips per face
        eastl::vector<eastl::vector<VkImageView>> mImageViews{};
        VkImageViewCreateInfo mImageViewCreateInfo{};

        bool mIsImageExternal{ false };

        bool mKeepInitializerResources{ false };
    };
}// namespace mikoto::renderer::vulkan

#endif// MIKOTO_VULKAN_TEXTURE2D_HH
