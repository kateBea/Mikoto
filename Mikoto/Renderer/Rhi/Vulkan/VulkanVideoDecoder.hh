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

#ifndef MIKOTO_RENDERER_VULKAN_VIDEO_DECODER_HH
#define MIKOTO_RENDERER_VULKAN_VIDEO_DECODER_HH

#include <volk.h>
#include <vk_mem_alloc.h>
#include <vk_video/vulkan_video_codec_h264std_decode.h>

#include <Video/H264Parser.hh>

#include <Renderer/Rhi/Buffer.hh>
#include <Renderer/Rhi/VideoDecoder.hh>

namespace mikoto::renderer::vulkan {

    class VideoImageStorage final : public rhi::DeviceObject {
    public:
        VideoImageStorage( const VkVideoProfileInfoKHR& profile, VkExtent2D extent, VkImageUsageFlags usage );
        ~VideoImageStorage() override;
        auto GetImage() const -> VkImage;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

        VkVideoDecodeH264ProfileInfoKHR mH264Profile{};
        VkVideoProfileInfoKHR mProfile{};
        VkImageCreateInfo mImageInfo{};
        VkImage mImage{};
        VkDeviceMemory mMemory{};
    };

    class VideoSurface final : public rhi::IVideoSurface {
    public:
        VideoSurface( const VkVideoProfileInfoKHR& profile, VkExtent2D extent, bool reference, bool coincident );
        ~VideoSurface() override;

        auto GetImage() const -> rhi::TextureHandle override;
        auto GetLumaImage() const -> rhi::TextureHandle override;
        auto GetChromaImage() const -> rhi::TextureHandle override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

        core::Ref<VideoImageStorage> mStorage{};
        rhi::TextureHandle mImage{};
        rhi::TextureHandle mLumaImage{};
        rhi::TextureHandle mChromaImage{};
        VkExtent2D mExtent{};
        VkImageUsageFlags mUsage{};
        bool mSampled{};
    };

    class VideoDecoder final : public rhi::IVideoDecoder {
    public:
        explicit VideoDecoder( const rhi::VideoDecoderDescription& description );

        MKT_NODISCARD auto GetMaxReferences() const -> core::u32 override;
        MKT_NODISCARD auto GetDpbSlotCount() const -> core::u32 override;
        MKT_NODISCARD auto CreateSurface( bool reference ) -> rhi::VideoSurfaceHandle override;

        MKT_NODISCARD auto UsesCoincidentOutput() const -> bool override;

        MKT_NODISCARD auto GetSession() const -> VkVideoSessionKHR;

        MKT_NODISCARD auto GetParameters() const -> VkVideoSessionParametersKHR;
        MKT_NODISCARD auto GetExtent() const -> VkExtent2D;

        MKT_NODISCARD auto CreateBitstream( eastl::span<const core::ubyte> bytes ) -> rhi::BufferHandle override;
        MKT_NODISCARD auto GetBitstreamLayout() const -> rhi::VideoBitstreamLayout override;

        MKT_NODISCARD auto MatchesConfiguration( const video::H264Picture& picture ) const -> bool;

        ~VideoDecoder() override;
    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        core::Ref<video::H264Picture> mConfiguration{};

        VkVideoSessionKHR mSession{};

        VkVideoProfileInfoKHR mProfile{};
        VkVideoCapabilitiesKHR mCapabilities{};
        VkVideoSessionParametersKHR mParameters{};
        VkVideoDecodeH264ProfileInfoKHR mH264Profile{};
        VkVideoDecodeCapabilitiesKHR mDecodeCapabilities{};
        VkVideoDecodeH264CapabilitiesKHR mH264Capabilities{};

        eastl::vector<VmaAllocation> mSessionMemory{};

        VkExtent2D mExtent{};
        core::u32 mMaxReferences{};
        core::u32 mDpbSlotCount{};

        bool mIsCoincident{};
    };
}

#endif
