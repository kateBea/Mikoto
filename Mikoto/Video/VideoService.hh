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

#ifndef MIKOTO_VIDEO_SERVICE_HH
#define MIKOTO_VIDEO_SERVICE_HH

#include <mutex>
#include <condition_variable>

#include <EASTL/deque.h>
#include <EASTL/shared_ptr.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>

#include <Assets/AssetVideo.hh>

#include <Material/SurfaceMaterial.hh>

#include <Video/H264Parser.hh>
#include <Video/VideoAudioStream.hh>

#include <Renderer/Rhi/VideoDecoder.hh>

namespace mikoto::asset { class AssetRegistryService; }
namespace mikoto::renderer { class CommandContext; struct FGVideoHandle; }
namespace mikoto::scene { class Scene; }

namespace mikoto::video {

    class AacDecoder;

    class DecodedVideoFrame final : public core::ReferenceCounted<DecodedVideoFrame> {
    public:
        renderer::rhi::VideoSurfaceHandle mSurface{};
        Timestamp mPresentationTime{};
        core::u64 mGeneration{};
        core::u32 mSlot{};
        bool mRecorded{};
    };

    class VideoPlayback final : public core::ReferenceCounted<VideoPlayback> {
    public:
        VideoPlayback( asset::AssetHandle<asset::VideoClip> clip, const PlaybackSettings& settings );

        auto Play() -> void;
        auto Stop() -> void;
        auto Pause() -> void;
        auto Seek( core::f64 seconds ) -> void;

        auto MarkPresented( core::u64 generation ) -> void;

        MKT_NODISCARD auto GetState() const -> PlaybackState;
        MKT_NODISCARD auto GetTimeSeconds() const -> core::f64;
        MKT_NODISCARD auto GetError() const -> const eastl::string&;
        MKT_NODISCARD auto GetClip() const -> core::Ref<asset::VideoClip>;
        MKT_NODISCARD auto HasPresented( core::u64 generation ) const -> bool;
        MKT_NODISCARD auto GetCurrentFrame() const -> core::Ref<DecodedVideoFrame>;
        MKT_NODISCARD auto GetOutputTexture() const -> renderer::rhi::TextureHandle;
        MKT_NODISCARD auto GetDecoder() const -> renderer::rhi::VideoDecoderHandle;

        ~VideoPlayback() override;

    private:
        friend class VideoService;

        struct ReferenceSlot {
            renderer::rhi::VideoReferencePicture mPicture{};
            bool mUsed{};
        };

        struct DecodeWork {
            renderer::rhi::VideoDecodeDescription mDescription{};
            core::Ref<DecodedVideoFrame> mFrame{};
        };

    private:
        core::Ref<asset::VideoClip> mClip{};
        asset::AssetHandle<asset::VideoClip> mAsset{};

        PlaybackSettings mSettings{};
        PlaybackState mState{ PlaybackState::eLoading };

        eastl::string mError{};
        eastl::shared_ptr<H264Parser> mParser{};
        eastl::unique_ptr<VideoAudioStream> mAudio{};
        eastl::shared_ptr<AacDecoder> mAudioDecoder{};
        eastl::deque<core::Ref<H264Picture>> mPictures{};

        eastl::deque<PcmBlock> mPendingPcm{};
        eastl::vector<core::Ref<DecodedVideoFrame>> mFramePool{};
        eastl::deque<core::Ref<DecodedVideoFrame>> mDisplayQueue{};
        
        eastl::vector<ReferenceSlot> mDpb{};
        eastl::vector<DecodeWork> mDecodeWork{};
        core::Ref<DecodedVideoFrame> mCurrentFrame{};
        
        renderer::rhi::VideoDecoderHandle mDecoder{};
        renderer::rhi::TextureHandle mOutput{};
        
        core::Ref<material::SurfaceMaterial> mOriginalMaterial{};
        core::Ref<material::SurfaceMaterial> mPlaybackMaterial{};
        
        OutputMap mOutputMap{ OutputMap::eBaseColor };
        
        core::u64 mEpoch{};
        core::u64 mGeneration{};
        core::f64 mTimeSeconds{};
        core::u64 mPreviewSample{};
        core::i32 mPreviousPocMsb{};
        core::i32 mPreviousPocLsb{};
        core::u64 mNextVideoSample{};
        core::u64 mNextAudioSample{};
        core::u64 mMaterialRevision{};
        core::u64 mComponentRevision{};
        core::i32 mFrameNumberOffset{};
        core::u64 mPresentedGeneration{};
        core::i32 mPreviousFrameNumber{};

        eastl::optional<core::f64> mSeekTime{};
        
        bool mPreview{ false };
        bool mJobPending{ false };
        bool mVideoEnded{ false };
        bool mAudioEnded{ false };
        bool mResetDecoder{ true };
        bool mStartedAudio{ false };
    };

    using VideoPlaybackHandle = core::Ref<VideoPlayback>;

    class VideoService final : public core::IService, public core::Singleton<VideoService> {
    public:
        VideoService() = default;
        ~VideoService() override;

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        auto Update( scene::Scene& scene, core::f64 timeStep ) -> void;
        
        auto ReleaseScene( scene::Scene& scene ) -> void;
        auto PrepareDecodeWork( const scene::Scene& scene ) -> void;
        auto RecordDecodeWork( const scene::Scene& scene, renderer::CommandContext& context, renderer::FGVideoHandle handle ) -> void;

        MKT_NODISCARD auto GetPlaybacks( const scene::Scene& scene ) const -> eastl::vector<VideoPlaybackHandle>;
        MKT_NODISCARD auto GetPlayback( const scene::Scene &scene, core::u64 entity ) const -> VideoPlaybackHandle;
        
        MKT_NODISCARD auto GetSceneRevision( const scene::Scene &scene ) const -> core::u64;
        MKT_NODISCARD auto GetMaterialOverride( const scene::Scene &scene, core::u64 entity ) const -> material::MaterialHandle;

    private:
        struct CompletedJob {
            core::u64 mEpoch{};
            core::u64 mNextVideoSample{};
            core::u64 mNextAudioSample{};
            
            VideoPlaybackHandle mPlayback{};
            
            eastl::vector<PcmBlock> mPcm{};
            eastl::vector<core::Ref<H264Picture>> mPictures{};
            
            eastl::string mError{};
            eastl::shared_ptr<AacDecoder> mAudioDecoder{};

            bool mAudioEnded{};
        };

        struct ScenePlayers {
            ankerl::unordered_dense::map<core::u64, VideoPlaybackHandle> mPlayers{};
            core::u64 mRevision{};
        };

    private:
        auto CollectJobs() -> void;
        auto RemovePlayback( VideoPlayback& playback ) -> void;
        auto ScheduleDecode( VideoPlaybackHandle playback ) -> void;
        auto ResetPlayback( VideoPlayback& playback, core::f64 seconds ) -> void;
        auto PreparePicture( VideoPlayback& playback, core::Ref<H264Picture> picture, core::Ref<DecodedVideoFrame> frame ) -> void;

    private:
        audio::AudioDevice* mAudioDevice{};
        renderer::rhi::IGpuDevice* mDevice{};
        
        std::mutex mJobsMutex{};
        core::u32 mActiveJobs{};
        std::condition_variable mJobsFinished{};
        
        eastl::vector<CompletedJob> mCompletedJobs{};
        
        
        bool mAcceptingJobs{ false };
        bool mMediaFoundationStarted{ false };

        ankerl::unordered_dense::map<const scene::Scene*, ScenePlayers> mScenes{};
    };
}

#endif
