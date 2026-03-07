//    Copyright 2025 ケイト
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

#include <utility>

#include <Logging/Logger.hh>

#include <Animation/SkinnedAnimation.hh>

namespace Mikoto {

    SkinnedAnimation::SkinnedAnimation( AnimationBuilder&& description )
        {}

    auto SkinnedAnimation::GetDuration() const -> float {
        return m_Duration;
    }

    auto SkinnedAnimation::GetName() const -> const std::string& {
        return m_Name;
    }

    auto SkinnedAnimation::GetOzzAnimation() -> ozz::animation::Animation* {
        return m_Animation.get();
    }

    auto SkinnedAnimation::ResolveSamplers() -> void {
        for ( const AnimationChannel& channel: m_Channels ) {
            AnimationSampler& sampler{ m_Samplers[channel.SamplerIndex] };

            switch ( channel.Path ) {
                case PathType::TRANSLATION: {
                    sampler.Positions.reserve( sampler.OutputsVec4.size() );

                    for ( const glm::vec4& v: sampler.OutputsVec4 ) {
                        sampler.Positions.emplace_back( v.x, v.y, v.z );
                    }

                    break;
                }

                case PathType::SCALE: {
                    sampler.Scales.reserve( sampler.OutputsVec4.size() );

                    for ( const glm::vec4& v: sampler.OutputsVec4 ) {
                        sampler.Scales.emplace_back( v.x, v.y, v.z );
                    }

                    break;
                }

                case PathType::ROTATION: {
                    // Quaternions have 4 components
                    const Size keyCount{ sampler.Outputs.size() / 4 };

                    sampler.Rotations.reserve( keyCount );

                    for ( Size i{}; i < keyCount; ++i ) {
                        const float x{ sampler.Outputs[i * 4 + 0] };
                        const float y{ sampler.Outputs[i * 4 + 1] };
                        const float z{ sampler.Outputs[i * 4 + 2] };
                        const float w{ sampler.Outputs[i * 4 + 3] };

                        // glTF stores quaternion as (x,y,z,w)
                        sampler.Rotations.emplace_back( x, y, z, w );
                    }

                    break;
                }
            }
        }
    }

    //auto SkinnedAnimation::BuildOzzStructures( const Skeleton& skeleton ) -> void {
    //    ResolveSamplers();

    //    // Construct raw animation
    //    const UInt32 jointCount{ skeleton.GetBoneCount() };

    //    m_RawAnimation.duration = m_Duration;
    //    m_RawAnimation.tracks.resize( jointCount );

    //    for ( const AnimationChannel& channel: m_Channels ) {
    //        const AnimationSampler& sampler{ m_Samplers[channel.SamplerIndex] };

    //        // find by name
    //        // Root of armature is included in channels we juist ignore it
    //        Int32 index{ skeleton.GetOzzBondeIndex( channel.JointIndex ) };
    //        if (index == -1) {
    //            continue;
    //        }

    //        auto& track{ m_RawAnimation.tracks[index] };

    //        switch ( channel.Path ) {
    //            case PathType::TRANSLATION: {
    //                for ( Size i{}; i < sampler.TimeStamps.size(); ++i ) {
    //                    ozz::animation::offline::RawAnimation::TranslationKey key{};

    //                    key.time = sampler.TimeStamps[i];

    //                    const Vec3F& pos{ sampler.Positions[i] };
    //                    key.value = ozz::math::Float3{ pos.x, pos.y, pos.z };

    //                    track.translations.push_back( key );
    //                }

    //                break;
    //            }

    //            case PathType::ROTATION: {
    //                for ( Size i{}; i < sampler.TimeStamps.size(); ++i ) {
    //                    ozz::animation::offline::RawAnimation::RotationKey key{};

    //                    key.time = sampler.TimeStamps[i];

    //                    const Quat& rot{ sampler.Rotations[i] };
    //                    key.value = ozz::math::Quaternion{
    //                        rot.x, rot.y, rot.z, rot.w
    //                    };

    //                    track.rotations.push_back( key );
    //                }

    //                break;
    //            }

    //            case PathType::SCALE: {
    //                for ( Size i{}; i < sampler.TimeStamps.size(); ++i ) {
    //                    ozz::animation::offline::RawAnimation::ScaleKey key{};

    //                    key.time = sampler.TimeStamps[i];

    //                    const Vec3F& scale{ sampler.Scales[i] };
    //                    key.value = ozz::math::Float3{
    //                        scale.x, scale.y, scale.z
    //                    };

    //                    track.scales.push_back( key );
    //                }

    //                break;
    //            }
    //        }
    //    }

    //    // Construct runtime animation
    //    if (!m_RawAnimation.Validate()) {
    //        MKT_CORE_LOGGER_ERROR( "Animation is not valid" );
    //        return;
    //    }

    //    m_Animation = m_AnimationBuilder( m_RawAnimation );

    //    // Skeleton and animation needs to match.
    //    if ( skeleton.GetOzzSkeleton()->num_joints() != m_Animation->num_tracks() ) {
    //        MKT_CORE_LOGGER_ERROR( "Skeleton joints count does not match animation tracks count" );
    //    }
    //}
}
