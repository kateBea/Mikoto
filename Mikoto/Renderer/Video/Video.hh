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

#ifndef MIKOTO_RENDERER_VIDEO_VIDEO_HH
#define MIKOTO_RENDERER_VIDEO_VIDEO_HH

#include <Material/SurfaceMaterial.hh>
#include <Video/VideoService.hh>
#include <Renderer/Rhi/VideoDecoder.hh>

namespace mikoto::renderer {

    enum class VideoOutputMap : core::u32 { eBaseColor, eEmissive };

    // Compressed storage is shared by render data using the same immutable clip.
    struct VideoBitstream final : public core::ReferenceCounted<VideoBitstream> {
        core::Ref<asset::VideoClip> mClip{};
        rhi::BufferHandle mBuffer{};
        eastl::vector<rhi::BufferRange> mPictures{};

        MKT_NODISCARD static auto Create( const core::Ref<asset::VideoClip>& clip,
            rhi::IVideoDecoder* decoder ) -> core::Ref<VideoBitstream>;
    };

    struct DecodedVideoFrame final : public core::ReferenceCounted<DecodedVideoFrame> {
        rhi::VideoSurfaceHandle mSurface{};
        video::Timestamp mPresentationTime{};
        core::u64 mGeneration{};
        core::u32 mSlot{};
        bool mRecorded{};
    };

    // Rendering resources belong to a component instance and can serve several view graphs.
    class VideoRenderData final : public core::ReferenceCounted<VideoRenderData> {
    public:
        explicit VideoRenderData( video::VideoPlaybackHandle playback );

        auto Prepare() -> void;
        auto UploadBitstream() -> void;
        auto UpdateMaterial( material::SurfaceMaterialHandle source, VideoOutputMap output ) -> void;
        auto MarkPresented( core::u64 generation ) -> void;

        MKT_NODISCARD auto GetPlayback() const -> const video::VideoPlaybackHandle&;
        MKT_NODISCARD auto GetCurrentFrame() const -> core::Ref<DecodedVideoFrame>;
        MKT_NODISCARD auto GetOutputTexture() const -> rhi::TextureHandle;
        MKT_NODISCARD auto GetMaterial() const -> material::MaterialHandle;
        MKT_NODISCARD auto GetRevision() const -> core::u64;
        MKT_NODISCARD auto HasPresented( core::u64 generation ) const -> bool;

    private:
        friend class VideoModule;

        struct ReferenceSlot {
            rhi::VideoReferencePicture mPicture{};
            bool mUsed{};
        };

        struct DecodeWork {
            rhi::VideoDecodeDescription mDescription{};
            core::Ref<DecodedVideoFrame> mFrame{};
        };

        auto ResetDecoder() -> void;
        auto PreparePicture( core::Ref<video::H264Picture> picture, core::Ref<DecodedVideoFrame> frame ) -> void;

        video::VideoPlaybackHandle mPlayback{};
        rhi::VideoDecoderHandle mDecoder{};
        core::Ref<VideoBitstream> mBitstream{};
        rhi::TextureHandle mOutput{};
        eastl::vector<core::Ref<DecodedVideoFrame>> mFramePool{};
        eastl::deque<core::Ref<DecodedVideoFrame>> mDisplayQueue{};
        eastl::vector<ReferenceSlot> mDpb{};
        eastl::vector<DecodeWork> mDecodeWork{};
        core::Ref<DecodedVideoFrame> mCurrentFrame{};
        material::SurfaceMaterialHandle mOriginalMaterial{};
        material::SurfaceMaterialHandle mPlaybackMaterial{};
        VideoOutputMap mOutputMap{ VideoOutputMap::eBaseColor };
        core::u64 mEpoch{ ~core::u64{} };
        core::u64 mGeneration{};
        core::u64 mPresentedGeneration{};
        core::u64 mMaterialRevision{};
        core::u64 mRevision{};
        core::i32 mPreviousPocMsb{};
        core::i32 mPreviousPocLsb{};
        core::i32 mPreviousFrameNumber{};
        core::i32 mFrameNumberOffset{};
        bool mResetDecoder{ true };
    };

    using VideoRenderDataHandle = core::Ref<VideoRenderData>;
}

#endif
