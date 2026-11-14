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

#ifndef MIKOTO_ANIMATION_KEYFRAME_CLIP_HH
#define MIKOTO_ANIMATION_KEYFRAME_CLIP_HH

#include <EASTL/span.h>
#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>

#include <yaml-cpp/node/node.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::animation {

    enum class KeyframeChannel : core::u8 {
        ePosition,
        eRotation,
        eScale,
        eLightColor,
        eLightIntensity,
        eCount
    };

    enum class KeyframeInterpolation : core::u8 {
        eStep,
        eLinear,
        eSmooth
    };

    struct Keyframe {
        core::i32 mFrame{};
        core::float3 mValue{};
        KeyframeInterpolation mInterpolation{ KeyframeInterpolation::eLinear };
    };

    class KeyframeTrack {
    public:
        // Sorted unique frames: inserting at an occupied frame replaces its key.
        auto SetKey( const Keyframe& key ) -> bool;
        auto RemoveKey( core::i32 frame ) -> bool;
        auto MoveKey( core::i32 from, core::i32 to ) -> bool;

        MKT_NODISCARD auto GetKeys() const -> eastl::span<const Keyframe>;
        MKT_NODISCARD auto FindKey( core::i32 frame ) const -> const Keyframe*;
        MKT_NODISCARD auto Sample( core::f64 frame, const core::float3& fallback ) const -> core::float3;

    private:
        eastl::vector<Keyframe> mKeys{};
    };

    struct KeyframeClip {
        static constexpr core::i32 mMaxFrame{ 100000 };
        eastl::string mName{ "New animation" };
        core::i32 mFrameRate{ 30 };
        core::i32 mLastFrame{ 90 };

        bool mLoop{ true };
        bool mPlayOnStart{ true };

        eastl::array<KeyframeTrack, static_cast<core::usize>( KeyframeChannel::eCount )> mTracks{};

        auto Sanitize() -> void;

        MKT_NODISCARD auto Advance( core::f64 frame, core::f64 seconds ) const -> core::f64;
        MKT_NODISCARD static auto ChannelName( KeyframeChannel channel ) -> core::cstr;

        /**
         * @brief Encodes this clip's settings and all animation-channel keys as YAML.
         * @returns YAML mapping containing the clip and its tracks.
         */
        MKT_NODISCARD auto EncodeKeyframeClip() const -> YAML::Node;

        /**
         * @brief Reads and validates YAML before replacing this clip.
         * @param node YAML mapping containing clip settings and tracks.
         * @details Sanitizes playback settings and validates all keys before updating
         * this clip. Leaves this clip unchanged if decoding fails.
         * @throws core::RuntimeException If tracks, channels or key values are invalid.
         * @throws YAML::Exception If a YAML value cannot be converted to its expected type.
         */
        auto DecodeKeyframeClip( const YAML::Node& node ) -> void;
    };
}// namespace mikoto::animation

#endif// MIKOTO_ANIMATION_KEYFRAME_CLIP_HH