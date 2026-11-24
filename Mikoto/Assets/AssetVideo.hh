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

        MKT_NODISCARD auto GetInfo() const -> const video::ClipInfo&;
        MKT_NODISCARD auto GetPath() const -> const filesystem::Path &;
        MKT_NODISCARD auto GetSamples( video::TrackKind track ) const -> eastl::span<const video::SampleInfo>;

        MKT_NODISCARD auto FindRandomAccessSample( core::f64 seconds ) const -> core::u64;
        MKT_NODISCARD auto ReadSample( video::TrackKind track, core::u64 index, video::EncodedPacket &packet ) const -> bool;

        MKT_NODISCARD auto CanEvict() const -> bool;
        MKT_NODISCARD auto GetResidentMemoryUsage() const -> AssetMemoryUsage;

    private:
        video::ClipInfo mInfo{};
        filesystem::Path mPath{};
        eastl::string mSourceBytes{};

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
