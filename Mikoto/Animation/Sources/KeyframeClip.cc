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

#include <cmath>
#include <string>

#include <EASTL/algorithm.h>

#include <glm/glm.hpp>
#include <glm/gtx/common.hpp>
#include <glm/gtx/compatibility.hpp>

#include <yaml-cpp/yaml.h>

#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Animation/KeyframeClip.hh>

namespace mikoto::animation {
    using namespace core;

    auto KeyframeTrack::SetKey( const Keyframe& key ) -> bool {
        if ( key.mFrame < 0 || key.mFrame > KeyframeClip::mMaxFrame ||
             key.mInterpolation > KeyframeInterpolation::eSmooth ) {
            return false;
        }

        const auto found{ eastl::lower_bound( mKeys.begin(), mKeys.end(), key.mFrame,
            []( const Keyframe& entry, core::i32 frame ) {
            return entry.mFrame < frame;
        } ) };

        if ( found != mKeys.end() && found->mFrame == key.mFrame ) {
            *found = key;
        }
        else {
            mKeys.insert( found, key );
        }

        return true;
    }

    auto KeyframeTrack::RemoveKey( core::i32 frame ) -> bool {
        const auto found{ eastl::lower_bound( mKeys.begin(), mKeys.end(), frame,
            []( const Keyframe& entry, core::i32 value ) {
            return entry.mFrame < value;
        } ) };

        if ( found == mKeys.end() || found->mFrame != frame ) {
            return false;
        }

        mKeys.erase( found );
        return true;
    }

    auto KeyframeTrack::MoveKey( core::i32 from, core::i32 to ) -> bool {
        const auto* key{ FindKey( from ) };

        if ( !key || to < 0 || to > KeyframeClip::mMaxFrame ) {
            return false;
        }

        if ( from != to && FindKey( to ) ) {
            // Dragging across another diamond must never silently erase it.
            return false;
        }

        Keyframe moved{ *key };
        moved.mFrame = to;
        // Insert before removal so allocation failure cannot lose the old key.
        if ( !SetKey( moved ) ) {
            return false;
        }

        if ( from != to ) {
            RemoveKey( from );
        }

        return true;
    }

    auto KeyframeTrack::FindKey( core::i32 frame ) const -> const Keyframe* {
        const auto found{ eastl::lower_bound( mKeys.begin(), mKeys.end(), frame,
            []( const Keyframe& entry, core::i32 value ) {
            return entry.mFrame < value;
        } ) };
        return found != mKeys.end() && found->mFrame == frame ? &*found : nullptr;
    }

    auto KeyframeTrack::GetKeys() const -> eastl::span<const Keyframe> {
        return { mKeys.data(), mKeys.size() };
    }

    auto KeyframeTrack::Sample( core::f64 frame, const core::float3& fallback ) const -> core::float3 {
        MKT_BEGIN_PROFILER();

        if ( mKeys.empty() ) {
            return fallback;
        }
        // Binary search avoids scanning a long clip for every object each frame.
        // Hold endpoint values outside the keyed interval; empty tracks leave
        // their property unchanged. Interpolation belongs to the outgoing key.
        const auto right{ eastl::upper_bound( mKeys.begin(), mKeys.end(), frame,
            []( core::f64 value, const Keyframe& entry ) {
            return value < entry.mFrame;
        } ) };

        if ( right == mKeys.begin() ) {
            return right->mValue;
        }

        const auto& left{ *( right - 1 ) };

        if ( right == mKeys.end() || left.mInterpolation == KeyframeInterpolation::eStep ) {
            return left.mValue;
        }

        core::f32 weight{ static_cast<core::f32>( ( frame - left.mFrame ) / ( right->mFrame - left.mFrame ) ) };

        if ( left.mInterpolation == KeyframeInterpolation::eSmooth ) {
            // Smoothstep is an ease-in/ease-out segment with zero endpoint slope.
            weight = weight * weight * ( 3.0f - 2.0f * weight );
        }

        return glm::mix( left.mValue, right->mValue, weight );
    }

    auto KeyframeClip::Sanitize() -> void {
        mFrameRate = glm::clamp( mFrameRate, 1, 240 );
        mLastFrame = glm::clamp( mLastFrame, 1, mMaxFrame );
        // Shortening the playback range keeps keys outside it for later editing.
    }

    auto KeyframeClip::Advance( core::f64 frame, core::f64 seconds ) const -> core::f64 {
        const core::f64 end{ static_cast<core::f64>( glm::clamp( mLastFrame, 1, mMaxFrame ) ) };
        frame = glm::clamp( frame, 0.0, end );

        if ( seconds <= 0.0 ) {
            return frame;
        }

        const core::f64 advance{ seconds * glm::clamp( mFrameRate, 1, 240 ) };
        return mLoop ? glm::fmod( frame + glm::fmod( advance, end ), end ) : glm::min( frame + advance, end );
    }

    auto KeyframeClip::ChannelName( KeyframeChannel channel ) -> cstr {
        switch ( channel ) {
            case KeyframeChannel::ePosition:
                return "Position";
            case KeyframeChannel::eRotation:
                return "Rotation (degrees)";
            case KeyframeChannel::eScale:
                return "Scale";
            case KeyframeChannel::eLightColor:
                return "Light color";
            case KeyframeChannel::eLightIntensity:
                return "Light intensity";
            default:
                return "Unknown";
        }
    }

    auto KeyframeClip::EncodeKeyframeClip() const -> YAML::Node {
        YAML::Node node{};
        node["Name"] = mName.c_str();
        node["FrameRate"] = mFrameRate;
        node["LastFrame"] = mLastFrame;
        node["Loop"] = mLoop;
        node["PlayOnStart"] = mPlayOnStart;
        node["Tracks"] = YAML::Node{ YAML::NodeType::Sequence };

        for ( core::usize index{}; index < mTracks.size(); ++index ) {
            YAML::Node track{};
            track["Channel"] = static_cast<core::u32>( index );
            track["Keys"] = YAML::Node{ YAML::NodeType::Sequence };

            for ( const auto& key: mTracks[index].GetKeys() ) {
                YAML::Node value{};
                value["Frame"] = key.mFrame;
                value["Interpolation"] = static_cast<core::i32>( key.mInterpolation );
                value["Value"].push_back( key.mValue.x );
                value["Value"].push_back( key.mValue.y );
                value["Value"].push_back( key.mValue.z );
                track["Keys"].push_back( value );
            }

            node["Tracks"].push_back( track );
        }

        return node;
    }

    auto KeyframeClip::DecodeKeyframeClip( const YAML::Node& node ) -> void {
        if ( !node.IsMap() || !node["Tracks"].IsSequence() ) {
            MKT_THROW_RUNTIME_ERROR( "Keyframe animation requires a Tracks sequence" );
        }

        // Keep the current clip intact until every track and key has been validated.
        KeyframeClip clip{};
        clip.mName = node["Name"].as<std::string>( "Animation" ).c_str();
        clip.mFrameRate = node["FrameRate"].as<core::i32>( 30 );
        clip.mLastFrame = node["LastFrame"].as<core::i32>( 90 );
        clip.mLoop = node["Loop"].as<bool>( true );
        clip.mPlayOnStart = node["PlayOnStart"].as<bool>( true );
        clip.Sanitize();

        for ( const auto& track: node["Tracks"] ) {
            const core::usize channel{ track["Channel"].as<core::usize>() };

            if ( channel >= clip.mTracks.size() || !track["Keys"].IsSequence() ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid keyframe animation channel or Keys sequence" );
            }

            for ( const auto& value: track["Keys"] ) {
                const auto components{ value["Value"] };
                const core::i32 interpolation{ value["Interpolation"].as<core::i32>( 1 ) };

                if ( !components.IsSequence() || components.size() != 3 || interpolation < 0 || interpolation > 2 ) {
                    MKT_THROW_RUNTIME_ERROR( "Invalid keyframe value or interpolation" );
                }

                const Keyframe key{
                    .mFrame = value["Frame"].as<core::i32>(),
                    .mValue = { components[0].as<core::f32>(), components[1].as<core::f32>(), components[2].as<core::f32>() },
                    .mInterpolation = static_cast<KeyframeInterpolation>( interpolation ),
                };

                if ( !clip.mTracks[channel].SetKey( key ) ) {
                    MKT_THROW_RUNTIME_ERROR( "Invalid keyframe frame or interpolation" );
                }
            }
        }

        *this = eastl::move( clip );
    }
}// namespace mikoto::animation
