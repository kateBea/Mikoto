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

    class IVideoDecoder : public DeviceObject {
    public:
        MKT_NODISCARD virtual auto CreateSurface( bool reference ) -> VideoSurfaceHandle = 0;

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
        VideoSurfaceHandle mOutput{};
        VideoReferencePicture mReconstruction{};
        eastl::fixed_vector<VideoReferencePicture, 16> mReferences{};
        bool mResetSession{};
    };
}

#endif