// Copyright 2026 ケイト. Licensed under the Apache License, Version 2.0.
#ifndef MIKOTO_ASSETS_ASSET_VIDEO_HH
#define MIKOTO_ASSETS_ASSET_VIDEO_HH

#include <EASTL/span.h>

#include <Assets/Asset.hh>

#include <Video/Types.hh>

namespace mikoto::asset {

    struct VideoLoadDescription {
        filesystem::Path mPath{};
        filesystem::FileHandle mFile{};
        LoadPriority mPriority{ LoadPriority::eNormal };
    };

    // The source and sample index are shared; each playback owns its own cursors.
    class VideoClip final : public core::ReferenceCounted<VideoClip> {
    public:
        explicit VideoClip( const VideoLoadDescription& description );

        auto GetInfo() const -> const video::ClipInfo&;
        auto GetPath() const -> const filesystem::Path&;
        auto GetSamples( video::TrackKind track ) const -> eastl::span<const video::SampleInfo>;
        auto ReadSample( video::TrackKind track, core::u64 index, video::EncodedPacket& packet ) const -> bool;
        auto FindRandomAccessSample( core::f64 seconds ) const -> core::u64;
        auto CanEvict() const -> bool;
        auto GetResidentMemoryUsage() const -> AssetMemoryUsage;

    private:
        filesystem::Path mPath{};
        eastl::string mSourceBytes{};
        video::ClipInfo mInfo{};
        eastl::vector<video::SampleInfo> mVideoSamples{};
        eastl::vector<video::SampleInfo> mAudioSamples{};
    };

    template<>
    struct AssetTraits<VideoClip> {
        using Asset = VideoClip;
        using Description = VideoLoadDescription;

        static constexpr AssetType kType{ AssetType::eVideo };
    };
}

#endif
