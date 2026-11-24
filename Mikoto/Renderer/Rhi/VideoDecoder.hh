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

#ifndef MIKOTO_RENDERER_RHI_VIDEO_DECODER_HH
#define MIKOTO_RENDERER_RHI_VIDEO_DECODER_HH

#include <EASTL/fixed_vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Resource.hh>

#include <Video/H264Parser.hh>

#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/Buffer.hh>

namespace mikoto::renderer::rhi {

    class IVideoSurface : public DeviceObject {
    public:
        virtual auto GetImage() const -> TextureHandle = 0;
        virtual auto GetLumaImage() const -> TextureHandle = 0;
        virtual auto GetChromaImage() const -> TextureHandle = 0;

        ~IVideoSurface() override = default;
    };

    using VideoSurfaceHandle = core::Ref<IVideoSurface>;

    struct VideoDecoderDescription {
        core::Ref<video::H264Picture> mConfiguration{};
    };

    struct VideoBitstreamLayout {
        core::usize mOffsetAlignment{ 1 };
        core::usize mSizeAlignment{ 1 };
        core::u32 mStartCodeSize{ 4 };
    };

    class IVideoDecoder : public DeviceObject {
    public:
        MKT_NODISCARD virtual auto CreateSurface( bool reference ) -> VideoSurfaceHandle = 0;

        /** Uploads a complete, aligned Annex-B stream for reuse by every picture decode. */
        MKT_NODISCARD virtual auto CreateBitstream( eastl::span<const core::ubyte> bytes ) -> BufferHandle = 0;

        /** Returns the picture alignment and Annex-B start-code size required by this decoder. */
        MKT_NODISCARD virtual auto GetBitstreamLayout() const -> VideoBitstreamLayout = 0;

        MKT_NODISCARD virtual auto GetMaxReferences() const -> core::u32 = 0;
        MKT_NODISCARD virtual auto GetDpbSlotCount() const -> core::u32 = 0;
        MKT_NODISCARD virtual auto UsesCoincidentOutput() const -> bool = 0;

        ~IVideoDecoder() override = default;
    };

    using VideoDecoderHandle = core::Ref<IVideoDecoder>;

    struct VideoReferencePicture {
        VideoSurfaceHandle mSurface{};
        core::u32 mSlot{};
        core::u32 mFrameNumber{};
        core::i32 mPictureOrderCount{};
        bool mLongTerm{};
    };

    struct VideoDecodeDescription {
        VideoDecoderHandle mDecoder{};
        core::Ref<video::H264Picture> mPicture{};
        BufferHandle mBitstream{};
        BufferRange mBitstreamRange{};
        VideoSurfaceHandle mOutput{};
        VideoReferencePicture mReconstruction{};
        eastl::fixed_vector<VideoReferencePicture, 16> mReferences{};
        bool mResetSession{};
    };
}

#endif