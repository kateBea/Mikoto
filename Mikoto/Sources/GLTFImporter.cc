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

#include <tiny_gltf.h>

#include <Assets/AssetsService.hh>
#include <Assets/GltfAnimImporter.hh>
#include <Common/String.hh>
#include <Filesystem/FileService.hh>
#include <Filesystem/FileSystem.hh>
#include <Library/Math/Math.hh>
#include <Logging/Logger.hh>
#include <Material/PBRMaterial.hh>
#include <Renderer/Core/RenderUtility.hh>
#include <Threading/ThreadUtility.hh>
#include <algorithm>
#include <ranges>

#//---------------------------------------------------------------------------//
// Initial gltf2ozz implementation author: Alexander Dzhoganov                //
// https://github.com/guillaumeblanc/ozz-animation/pull/70                    //
//----------------------------------------------------------------------------//

#include <cassert>
#include <cstring>

#include "ozz/animation/offline/raw_animation_utils.h"
#include "ozz/animation/offline/tools/import2ozz.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/containers/map.h"
#include "ozz/base/containers/set.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/log.h"
#include "ozz/base/maths/math_ex.h"
#include "ozz/base/maths/simd_math.h"

#define TINYGLTF_IMPLEMENTATION

// No support for image loading or writing
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4702 )// unreachable code
#pragma warning( disable : 4267 )// conversion from 'size_t' to 'type'
#endif                           // _MSC_VER

#ifdef _MSC_VER
#pragma warning( pop )
#endif// _MSC_VER


namespace Mikoto {

    static inline int32_t GetTypeSizeInBytes( uint32_t ty ) {
        if ( ty == TINYGLTF_TYPE_SCALAR ) {
            return 1;
        } else if ( ty == TINYGLTF_TYPE_VEC2 ) {
            return 2;
        } else if ( ty == TINYGLTF_TYPE_VEC3 ) {
            return 3;
        } else if ( ty == TINYGLTF_TYPE_VEC4 ) {
            return 4;
        } else if ( ty == TINYGLTF_TYPE_MAT2 ) {
            return 4;
        } else if ( ty == TINYGLTF_TYPE_MAT3 ) {
            return 9;
        } else if ( ty == TINYGLTF_TYPE_MAT4 ) {
            return 16;
        } else {
            // Unknown componenty type
            return -1;
        }
    }

    template<typename _VectorType>
    bool FixupNames( _VectorType& _data, const char* _pretty_name,
                     const char* _prefix_name ) {
        ozz::set<std::string> names;
        for ( size_t i = 0; i < _data.size(); ++i ) {
            bool renamed = false;
            typename _VectorType::const_reference data = _data[i];

            std::string name( data.name.c_str() );

            // Fixes unnamed animations.
            if ( name.length() == 0 ) {
                renamed = true;
                name = _prefix_name;
                name += std::to_string( i );
            }

            // Fixes duplicated names, while it has duplicates
            for ( auto it = names.find( name ); it != names.end(); it = names.find( name ) ) {
                renamed = true;
                name += "_";
                name += std::to_string( i );
            }

            // Update names index.
            if ( !names.insert( name ).second ) {
                assert( false && "Algorithm must ensure no duplicated animation names." );
            }

            if ( renamed ) {
                ozz::log::LogV() << _pretty_name << " #" << i << " with name \""
                                 << data.name << "\" was renamed to \"" << name
                                 << "\" in order to avoid duplicates." << std::endl;

                // Actually renames tinygltf data.
                _data[i].name = name;
            }
        }

        return true;
    }

    // Returns the address of a gltf buffer given an accessor.
    // Performs basic checks to ensure the data is in the correct format
    template<typename T>
    ozz::span<const T> BufferView( const tinygltf::Model& _model,
                                   const tinygltf::Accessor& _accessor ) {
        const int32_t component_size =
                tinygltf::GetComponentSizeInBytes( _accessor.componentType );
        const int32_t element_size =
                component_size * GetTypeSizeInBytes( _accessor.type );
        if ( element_size != sizeof( T ) ) {
            ozz::log::Err() << "Invalid buffer view access. Expected element size '"
                            << sizeof( T ) << " got " << element_size << " instead."
                            << std::endl;
            return ozz::span<const T>();
        }

        const tinygltf::BufferView& bufferView =
                _model.bufferViews[_accessor.bufferView];
        const tinygltf::Buffer& buffer = _model.buffers[bufferView.buffer];
        const T* begin = reinterpret_cast<const T*>(
                buffer.data.data() + bufferView.byteOffset + _accessor.byteOffset );
        return ozz::span<const T>( begin, _accessor.count );
    }

    // Samples a linear animation channel
    // There is an exact mapping between gltf and ozz keyframes so we just copy
    // everything over.
    template<typename _KeyframesType>
    bool SampleLinearChannel( const tinygltf::Model& _model,
                              const tinygltf::Accessor& _output,
                              const ozz::span<const float>& _timestamps,
                              _KeyframesType* _keyframes ) {
        const size_t gltf_keys_count = _output.count;

        if ( gltf_keys_count == 0 ) {
            _keyframes->clear();
            return true;
        }

        typedef typename _KeyframesType::value_type::Value ValueType;
        const ozz::span<const ValueType> values =
                BufferView<ValueType>( _model, _output );
        if ( values.size_bytes() / sizeof( ValueType ) != gltf_keys_count ||
             _timestamps.size() != gltf_keys_count ) {
            ozz::log::Err() << "gltf format error, inconsistent number of keys."
                            << std::endl;
            return false;
        }

        _keyframes->reserve( _output.count );
        for ( size_t i = 0; i < _output.count; ++i ) {
            const typename _KeyframesType::value_type key{ _timestamps[i], values[i] };
            _keyframes->push_back( key );
        }

        return true;
    }

    // Samples a step animation channel
    // There are twice-1 as many ozz keyframes as gltf keyframes
    template<typename _KeyframesType>
    bool SampleStepChannel( const tinygltf::Model& _model,
                            const tinygltf::Accessor& _output,
                            const ozz::span<const float>& _timestamps,
                            _KeyframesType* _keyframes ) {
        const size_t gltf_keys_count = _output.count;

        if ( gltf_keys_count == 0 ) {
            _keyframes->clear();
            return true;
        }

        typedef typename _KeyframesType::value_type::Value ValueType;
        const ozz::span<const ValueType> values =
                BufferView<ValueType>( _model, _output );
        if ( values.size_bytes() / sizeof( ValueType ) != gltf_keys_count ||
             _timestamps.size() != gltf_keys_count ) {
            ozz::log::Err() << "gltf format error, inconsistent number of keys."
                            << std::endl;
            return false;
        }

        // A step is created with 2 consecutive keys. Last step is a single key.
        size_t numKeyframes = gltf_keys_count * 2 - 1;
        _keyframes->resize( numKeyframes );

        for ( size_t i = 0; i < _output.count; i++ ) {
            typename _KeyframesType::reference key = _keyframes->at( i * 2 );
            key.time = _timestamps[i];
            key.value = values[i];

            if ( i < _output.count - 1 ) {
                typename _KeyframesType::reference next_key = _keyframes->at( i * 2 + 1 );
                next_key.time = nexttowardf( _timestamps[i + 1], 0.f );
                next_key.value = values[i];
            }
        }

        return true;
    }

    // Samples a hermite spline in the form
    // p(t) = (2t^3 - 3t^2 + 1)p0 + (t^3 - 2t^2 + t)m0 + (-2t^3 + 3t^2)p1 + (t^3 -
    // t^2)m1 where t is a value between 0 and 1 p0 is the starting point at t = 0
    // m0 is the scaled starting tangent at t = 0
    // p1 is the ending point at t = 1
    // m1 is the scaled ending tangent at t = 1
    // p(t) is the resulting point value
    template<typename T>
    T SampleHermiteSpline( float _alpha, const T& p0, const T& m0, const T& p1,
                           const T& m1 ) {
        assert( _alpha >= 0.f && _alpha <= 1.f );

        const float t1 = _alpha;
        const float t2 = _alpha * _alpha;
        const float t3 = t2 * _alpha;

        // a = 2t^3 - 3t^2 + 1
        const float a = 2.0f * t3 - 3.0f * t2 + 1.0f;
        // b = t^3 - 2t^2 + t
        const float b = t3 - 2.0f * t2 + t1;
        // c = -2t^3 + 3t^2
        const float c = -2.0f * t3 + 3.0f * t2;
        // d = t^3 - t^2
        const float d = t3 - t2;

        // p(t) = a * p0 + b * m0 + c * p1 + d * m1
        T pt = p0 * a + m0 * b + p1 * c + m1 * d;
        return pt;
    }

    // Samples a cubic-spline channel
    // the number of keyframes is determined from the animation duration and given
    // sample rate
    template<typename _KeyframesType>
    bool SampleCubicSplineChannel( const tinygltf::Model& _model,
                                   const tinygltf::Accessor& _output,
                                   const ozz::span<const float>& _timestamps,
                                   float _sampling_rate, float _duration,
                                   _KeyframesType* _keyframes ) {
        ( void )_duration;

        assert( _output.count % 3 == 0 );
        size_t gltf_keys_count = _output.count / 3;

        if ( gltf_keys_count == 0 ) {
            _keyframes->clear();
            return true;
        }

        typedef typename _KeyframesType::value_type::Value ValueType;
        const ozz::span<const ValueType> values =
                BufferView<ValueType>( _model, _output );
        if ( values.size_bytes() / ( sizeof( ValueType ) * 3 ) != gltf_keys_count ||
             _timestamps.size() != gltf_keys_count ) {
            ozz::log::Err() << "gltf format error, inconsistent number of keys."
                            << std::endl;
            return false;
        }

        // Iterate keyframes at _sampling_rate steps, between first and last time
        // stamps.
        ozz::animation::offline::FixedRateSamplingTime fixed_it(
                _timestamps[gltf_keys_count - 1] - _timestamps[0], _sampling_rate );
        _keyframes->resize( fixed_it.num_keys() );
        size_t cubic_key0 = 0;
        for ( size_t k = 0; k < fixed_it.num_keys(); ++k ) {
            const float time = fixed_it.time( k ) + _timestamps[0];

            // Creates output key.
            typename _KeyframesType::value_type key;
            key.time = time;

            // Makes sure time is in between the correct cubic keyframes.
            while ( _timestamps[cubic_key0 + 1] < time ) {
                cubic_key0++;
            }
            assert( _timestamps[cubic_key0] <= time &&
                    time <= _timestamps[cubic_key0 + 1] );

            // Interpolate cubic key
            const float t0 = _timestamps[cubic_key0];    // keyframe before time
            const float t1 = _timestamps[cubic_key0 + 1];// keyframe after time
            const float alpha = ( time - t0 ) / ( t1 - t0 );
            const ValueType& p0 = values[cubic_key0 * 3 + 1];
            const ValueType m0 = values[cubic_key0 * 3 + 2] * ( t1 - t0 );
            const ValueType& p1 = values[( cubic_key0 + 1 ) * 3 + 1];
            const ValueType m1 = values[( cubic_key0 + 1 ) * 3] * ( t1 - t0 );
            key.value = SampleHermiteSpline( alpha, p0, m0, p1, m1 );

            // Pushes interpolated key.
            _keyframes->at( k ) = key;
        }

        return true;
    }

    template<typename _KeyframesType>
    bool SampleChannel( const tinygltf::Model& _model,
                        const std::string& _interpolation,
                        const tinygltf::Accessor& _output,
                        const ozz::span<const float>& _timestamps,
                        float _sampling_rate, float _duration,
                        _KeyframesType* _keyframes ) {
        bool valid = false;
        if ( _interpolation == "LINEAR" ) {
            valid = SampleLinearChannel( _model, _output, _timestamps, _keyframes );
        } else if ( _interpolation == "STEP" ) {
            valid = SampleStepChannel( _model, _output, _timestamps, _keyframes );
        } else if ( _interpolation == "CUBICSPLINE" ) {
            valid = SampleCubicSplineChannel( _model, _output, _timestamps,
                                              _sampling_rate, _duration, _keyframes );
        } else {
            ozz::log::Err() << "Invalid or unknown interpolation type '"
                            << _interpolation << "'." << std::endl;
            valid = false;
        }

        // Check if sorted (increasing time, might not be stricly increasing).
        if ( valid ) {
            valid = std::is_sorted( _keyframes->begin(), _keyframes->end(),
                                    []( typename _KeyframesType::const_reference _a,
                                        typename _KeyframesType::const_reference _b ) {
                                        return _a.time < _b.time;
                                    } );
            if ( !valid ) {
                ozz::log::Log()
                        << "gltf format error, keyframes are not sorted in increasing order."
                        << std::endl;
            }
        }

        // Remove keyframes with strictly equal times, keeping the first one.
        if ( valid ) {
            auto new_end = std::unique( _keyframes->begin(), _keyframes->end(),
                                        []( typename _KeyframesType::const_reference _a,
                                            typename _KeyframesType::const_reference _b ) {
                                            return _a.time == _b.time;
                                        } );
            if ( new_end != _keyframes->end() ) {
                _keyframes->erase( new_end, _keyframes->end() );

                ozz::log::Log() << "gltf format error, keyframe times are not unique. "
                                   "Imported data were modified to remove keyframes at "
                                   "consecutive equivalent times."
                                << std::endl;
            }
        }
        return valid;
    }

    ozz::animation::offline::RawAnimation::TranslationKey
    CreateTranslationRestPoseKey( const tinygltf::Node& _node ) {
        ozz::animation::offline::RawAnimation::TranslationKey key;
        key.time = 0.0f;

        if ( _node.translation.empty() ) {
            key.value = ozz::math::Float3::zero();
        } else {
            key.value = ozz::math::Float3( static_cast<float>( _node.translation[0] ),
                                           static_cast<float>( _node.translation[1] ),
                                           static_cast<float>( _node.translation[2] ) );
        }

        return key;
    }

    ozz::animation::offline::RawAnimation::RotationKey CreateRotationRestPoseKey(
            const tinygltf::Node& _node ) {
        ozz::animation::offline::RawAnimation::RotationKey key;
        key.time = 0.0f;

        if ( _node.rotation.empty() ) {
            key.value = ozz::math::Quaternion::identity();
        } else {
            key.value = ozz::math::Quaternion( static_cast<float>( _node.rotation[0] ),
                                               static_cast<float>( _node.rotation[1] ),
                                               static_cast<float>( _node.rotation[2] ),
                                               static_cast<float>( _node.rotation[3] ) );
        }
        return key;
    }

    ozz::animation::offline::RawAnimation::ScaleKey CreateScaleRestPoseKey(
            const tinygltf::Node& _node ) {
        ozz::animation::offline::RawAnimation::ScaleKey key;
        key.time = 0.0f;

        if ( _node.scale.empty() ) {
            key.value = ozz::math::Float3::one();
        } else {
            key.value = ozz::math::Float3( static_cast<float>( _node.scale[0] ),
                                           static_cast<float>( _node.scale[1] ),
                                           static_cast<float>( _node.scale[2] ) );
        }
        return key;
    }

    // Creates the default transform for a gltf node
    bool CreateNodeTransform( const tinygltf::Node& _node,
                              ozz::math::Transform* _transform ) {
        *_transform = ozz::math::Transform::identity();
        if ( !_node.matrix.empty() ) {
            const ozz::math::Float4x4 matrix = {
                { ozz::math::simd_float4::Load( static_cast<float>( _node.matrix[0] ),
                                                static_cast<float>( _node.matrix[1] ),
                                                static_cast<float>( _node.matrix[2] ),
                                                static_cast<float>( _node.matrix[3] ) ),
                  ozz::math::simd_float4::Load( static_cast<float>( _node.matrix[4] ),
                                                static_cast<float>( _node.matrix[5] ),
                                                static_cast<float>( _node.matrix[6] ),
                                                static_cast<float>( _node.matrix[7] ) ),
                  ozz::math::simd_float4::Load( static_cast<float>( _node.matrix[8] ),
                                                static_cast<float>( _node.matrix[9] ),
                                                static_cast<float>( _node.matrix[10] ),
                                                static_cast<float>( _node.matrix[11] ) ),
                  ozz::math::simd_float4::Load( static_cast<float>( _node.matrix[12] ),
                                                static_cast<float>( _node.matrix[13] ),
                                                static_cast<float>( _node.matrix[14] ),
                                                static_cast<float>( _node.matrix[15] ) ) }
            };

            if ( !ToAffine( matrix, _transform ) ) {
                ozz::log::Err() << "Failed to extract transformation from node \""
                                << _node.name << "\"." << std::endl;
                return false;
            }
            return true;
        }

        if ( !_node.translation.empty() ) {
            _transform->translation =
                    ozz::math::Float3( static_cast<float>( _node.translation[0] ),
                                       static_cast<float>( _node.translation[1] ),
                                       static_cast<float>( _node.translation[2] ) );
        }
        if ( !_node.rotation.empty() ) {
            _transform->rotation =
                    ozz::math::Quaternion( static_cast<float>( _node.rotation[0] ),
                                           static_cast<float>( _node.rotation[1] ),
                                           static_cast<float>( _node.rotation[2] ),
                                           static_cast<float>( _node.rotation[3] ) );
        }
        if ( !_node.scale.empty() ) {
            _transform->scale = ozz::math::Float3( static_cast<float>( _node.scale[0] ),
                                                   static_cast<float>( _node.scale[1] ),
                                                   static_cast<float>( _node.scale[2] ) );
        }

        return true;
    }


    class GltfAnimImporter : public ozz::animation::offline::OzzImporter {
    public:
        GltfAnimImporter() {
            // We don't care about image data but we have to provide this callback
            // because we're not loading the stb library
            auto image_loader = []( tinygltf::Image*, const int, std::string*,
                                    std::string*, int, int, const unsigned char*, int,
                                    void* ) { return true; };
            m_loader.SetImageLoader( image_loader, NULL );
        }

    private:
        bool Load( const char* _filename ) override {
            bool success = false;
            std::string errors;
            std::string warnings;

            // Finds file extension.
            const char* separator = std::strrchr( _filename, '.' );
            const char* ext = separator != NULL ? separator + 1 : "";

            // Tries to guess whether the input is a gltf json or a glb binary based on
            // the file extension
            if ( std::strcmp( ext, "glb" ) == 0 ) {
                success =
                        m_loader.LoadBinaryFromFile( &m_model, &errors, &warnings, _filename );
            } else {
                if ( std::strcmp( ext, "gltf" ) != 0 ) {
                    ozz::log::Log() << "Unknown file extension '" << ext
                                    << "', assuming a JSON-formatted gltf." << std::endl;
                }

                success =
                        m_loader.LoadASCIIFromFile( &m_model, &errors, &warnings, _filename );
            }

            // Prints any errors or warnings emitted by the loader
            if ( !warnings.empty() ) {
                ozz::log::Log() << "glTF parsing warnings: " << warnings << std::endl;
            }

            if ( !errors.empty() ) {
                ozz::log::Err() << "glTF parsing errors: " << errors << std::endl;
            }

            if ( success ) {
                ozz::log::Log() << "glTF parsed successfully." << std::endl;
            }

            if ( success ) {
                success &= FixupNames( m_model.scenes, "Scene", "scene_" );
                success &= FixupNames( m_model.nodes, "Node", "node_" );
                success &= FixupNames( m_model.animations, "Animation", "animation_" );
            }

            return success;
        }

        // Find all unique root joints of skeletons used by given skins and add them
        // to `roots`
        void FindSkinRootJointIndices( const ozz::vector<tinygltf::Skin>& skins,
                                       ozz::vector<int>& roots ) {
            static constexpr int no_parent = -1;
            static constexpr int visited = -2;
            ozz::vector<int> parents( m_model.nodes.size(), no_parent );
            for ( int node = 0; node < static_cast<int>( m_model.nodes.size() ); node++ ) {
                for ( int child: m_model.nodes[node].children ) {
                    parents[child] = node;
                }
            }

            for ( const tinygltf::Skin& skin: skins ) {
                if ( skin.joints.empty() ) {
                    continue;
                }

                if ( skin.skeleton != -1 ) {
                    parents[skin.skeleton] = visited;
                    roots.push_back( skin.skeleton );
                    continue;
                }

                int root = skin.joints[0];
                while ( root != visited && parents[root] != no_parent ) {
                    root = parents[root];
                }
                if ( root != visited ) {
                    roots.push_back( root );
                }
            }
        }

        bool Import( ozz::animation::offline::RawSkeleton* _skeleton,
                     const NodeType& _types ) override {
            ( void )_types;

            if ( m_model.scenes.empty() ) {
                ozz::log::Err() << "No scenes found." << std::endl;
                return false;
            }

            // If no default scene has been set then take the first one spec does not
            // disallow gltfs without a default scene but it makes more sense to keep
            // going instead of throwing an error here
            int defaultScene = m_model.defaultScene;
            if ( defaultScene == -1 ) {
                defaultScene = 0;
            }

            tinygltf::Scene& scene = m_model.scenes[defaultScene];
            ozz::log::LogV() << "Importing from default scene #" << defaultScene
                             << " with name \"" << scene.name << "\"." << std::endl;

            if ( scene.nodes.empty() ) {
                ozz::log::Err() << "Scene has no node." << std::endl;
                return false;
            }

            // Get all the skins belonging to this scene
            ozz::vector<int> roots;
            ozz::vector<tinygltf::Skin> skins = GetSkinsForScene( scene );
            if ( skins.empty() ) {
                ozz::log::Log() << "No skin exists in the scene, the whole scene graph "
                                   "will be considered as a skeleton."
                                << std::endl;
                // Uses all scene nodes.
                for ( auto& node: scene.nodes ) {
                    roots.push_back( node );
                }
            } else {
                if ( skins.size() > 1 ) {
                    ozz::log::Log() << "Multiple skins exist in the scene, they will all "
                                       "be exported to a single skeleton."
                                    << std::endl;
                }

                // Uses all skins roots.
                FindSkinRootJointIndices( skins, roots );
            }

            // Remove nodes listed multiple times.
            std::sort( roots.begin(), roots.end() );
            roots.erase( std::unique( roots.begin(), roots.end() ), roots.end() );

            // Traverses the scene graph and record all joints starting from the roots.
            _skeleton->roots.resize( roots.size() );
            for ( size_t i = 0; i < roots.size(); ++i ) {
                const tinygltf::Node& root_node = m_model.nodes[roots[i]];
                ozz::animation::offline::RawSkeleton::Joint& root_joint =
                        _skeleton->roots[i];
                if ( !ImportNode( root_node, &root_joint ) ) {
                    return false;
                }
            }

            if ( !_skeleton->Validate() ) {
                ozz::log::Err() << "Output skeleton failed validation. This is likely an "
                                   "implementation issue."
                                << std::endl;
                return false;
            }

            return true;
        }

        // Recursively import a node's children
        bool ImportNode( const tinygltf::Node& _node,
                         ozz::animation::offline::RawSkeleton::Joint* _joint ) {
            // Names joint.
            _joint->name = _node.name.c_str();

            // Fills transform.
            if ( !CreateNodeTransform( _node, &_joint->transform ) ) {
                return false;
            }

            // Allocates all children at once.
            _joint->children.resize( _node.children.size() );

            // Fills each child information.
            for ( size_t i = 0; i < _node.children.size(); ++i ) {
                const tinygltf::Node& child_node = m_model.nodes[_node.children[i]];
                ozz::animation::offline::RawSkeleton::Joint& child_joint =
                        _joint->children[i];

                if ( !ImportNode( child_node, &child_joint ) ) {
                    return false;
                }
            }

            return true;
        }

        // Returns all animations in the gltf document.
        AnimationNames GetAnimationNames() override {
            AnimationNames animNames;
            for ( size_t i = 0; i < m_model.animations.size(); ++i ) {
                tinygltf::Animation& animation = m_model.animations[i];
                assert( animation.name.length() != 0 );
                animNames.push_back( animation.name.c_str() );
            }

            return animNames;
        }

        bool Import( const char* _animation_name,
                     const ozz::animation::Skeleton& skeleton, float _sampling_rate,
                     ozz::animation::offline::RawAnimation* _animation ) override {
            if ( _sampling_rate == 0.0f ) {
                _sampling_rate = 30.0f;

                static bool samplingRateWarn = false;
                if ( !samplingRateWarn ) {
                    ozz::log::LogV() << "The animation sampling rate is set to 0 "
                                        "(automatic) but glTF does not carry scene frame "
                                        "rate information. Assuming a sampling rate of "
                                     << _sampling_rate << "hz." << std::endl;

                    samplingRateWarn = true;
                }
            }

            // Find the corresponding gltf animation
            std::vector<tinygltf::Animation>::const_iterator gltf_animation =
                    std::find_if( begin( m_model.animations ), end( m_model.animations ),
                                  [_animation_name]( const tinygltf::Animation& _animation ) {
                                      return _animation.name == _animation_name;
                                  } );
            assert( gltf_animation != end( m_model.animations ) );

            _animation->name = gltf_animation->name.c_str();

            // Animation duration is determined during sampling from the duration of the
            // longest channel
            _animation->duration = 0.0f;

            const int num_joints = skeleton.num_joints();
            _animation->tracks.resize( num_joints );

            // gltf stores animations by splitting them in channels
            // where each channel targets a node's property i.e. translation, rotation
            // or scale. ozz expects animations to be stored per joint so we create a
            // map where we record the associated channels for each joint
            ozz::cstring_map<std::vector<const tinygltf::AnimationChannel*>>
                    channels_per_joint;

            for ( const tinygltf::AnimationChannel& channel: gltf_animation->channels ) {
                // Reject if no node is targetted.
                if ( channel.target_node == -1 ) {
                    continue;
                }

                // Reject if path isn't about skeleton animation.
                bool valid_target = false;
                for ( const char* path: { "translation", "rotation", "scale" } ) {
                    valid_target |= channel.target_path == path;
                }
                if ( !valid_target ) {
                    continue;
                }

                const tinygltf::Node& target_node = m_model.nodes[channel.target_node];
                channels_per_joint[target_node.name.c_str()].push_back( &channel );
            }

            // For each joint get all its associated channels, sample them and record
            // the samples in the joint track
            const auto& joint_names = skeleton.joint_names();
            for ( int i = 0; i < num_joints; i++ ) {
                auto& channels = channels_per_joint[joint_names[i]];
                auto& track = _animation->tracks[i];

                for ( auto& channel: channels ) {
                    auto& sampler = gltf_animation->samplers[channel->sampler];
                    if ( !SampleAnimationChannel( m_model, sampler, channel->target_path,
                                                  _sampling_rate, &_animation->duration,
                                                  &track ) ) {
                        return false;
                    }
                }

                const tinygltf::Node* node = FindNodeByName( joint_names[i] );
                assert( node != nullptr );

                // Pads the rest pose transform for any joints which do not have an
                // associated channel for this animation
                if ( track.translations.empty() ) {
                    track.translations.push_back( CreateTranslationRestPoseKey( *node ) );
                }
                if ( track.rotations.empty() ) {
                    track.rotations.push_back( CreateRotationRestPoseKey( *node ) );
                }
                if ( track.scales.empty() ) {
                    track.scales.push_back( CreateScaleRestPoseKey( *node ) );
                }
            }

            ozz::log::LogV() << "Processed animation '" << _animation->name
                             << "' (tracks: " << _animation->tracks.size()
                             << ", duration: " << _animation->duration << "s)."
                             << std::endl;

            if ( !_animation->Validate() ) {
                ozz::log::Err() << "Animation '" << _animation->name
                                << "' failed validation." << std::endl;
                return false;
            }

            return true;
        }

        bool SampleAnimationChannel(
                const tinygltf::Model& _model, const tinygltf::AnimationSampler& _sampler,
                const std::string& _target_path, float _sampling_rate, float* _duration,
                ozz::animation::offline::RawAnimation::JointTrack* _track ) {
            // Validate interpolation type.
            if ( _sampler.interpolation.empty() ) {
                ozz::log::Err() << "Invalid sampler interpolation." << std::endl;
                return false;
            }

            auto& input = m_model.accessors[_sampler.input];
            assert( input.maxValues.size() == 1 );

            // The max[0] property of the input accessor is the animation duration
            // this is required to be present by the spec:
            // "Animation Sampler's input accessor must have min and max properties
            // defined."
            const float duration = static_cast<float>( input.maxValues[0] );

            // If this channel's duration is larger than the animation's duration
            // then increase the animation duration to match.
            if ( duration > *_duration ) {
                *_duration = duration;
            }

            assert( input.type == TINYGLTF_TYPE_SCALAR );
            auto& _output = m_model.accessors[_sampler.output];
            assert( _output.type == TINYGLTF_TYPE_VEC3 ||
                    _output.type == TINYGLTF_TYPE_VEC4 );

            const ozz::span<const float> timestamps = BufferView<float>( _model, input );
            if ( timestamps.empty() ) {
                return true;
            }

            // Builds keyframes.
            bool valid = false;
            if ( _target_path == "translation" ) {
                valid =
                        SampleChannel( m_model, _sampler.interpolation, _output, timestamps,
                                       _sampling_rate, duration, &_track->translations );
            } else if ( _target_path == "rotation" ) {
                valid =
                        SampleChannel( m_model, _sampler.interpolation, _output, timestamps,
                                       _sampling_rate, duration, &_track->rotations );
                if ( valid ) {
                    // Normalize quaternions.
                    for ( auto& key: _track->rotations ) {
                        key.value = ozz::math::Normalize( key.value );
                    }
                }
            } else if ( _target_path == "scale" ) {
                valid =
                        SampleChannel( m_model, _sampler.interpolation, _output, timestamps,
                                       _sampling_rate, duration, &_track->scales );
            } else {
                assert( false && "Invalid target path" );
            }

            return valid;
        }

        // Returns all skins belonging to a given gltf scene
        ozz::vector<tinygltf::Skin> GetSkinsForScene(
                const tinygltf::Scene& _scene ) const {
            ozz::set<int> open;
            ozz::set<int> found;

            for ( int nodeIndex: _scene.nodes ) {
                open.insert( nodeIndex );
            }

            while ( !open.empty() ) {
                int nodeIndex = *open.begin();
                found.insert( nodeIndex );
                open.erase( nodeIndex );

                auto& node = m_model.nodes[nodeIndex];
                for ( int childIndex: node.children ) {
                    open.insert( childIndex );
                }
            }

            ozz::vector<tinygltf::Skin> skins;
            for ( const tinygltf::Skin& skin: m_model.skins ) {
                if ( !skin.joints.empty() && found.find( skin.joints[0] ) != found.end() ) {
                    skins.push_back( skin );
                }
            }

            return skins;
        }

        const tinygltf::Node* FindNodeByName( const std::string& _name ) const {
            for ( const tinygltf::Node& node: m_model.nodes ) {
                if ( node.name == _name ) {
                    return &node;
                }
            }

            return nullptr;
        }

        // no support for user-defined tracks
        NodeProperties GetNodeProperties( const char* ) override {
            return NodeProperties();
        }
        bool Import( const char*, const char*, const char*, NodeProperty::Type, float,
                     ozz::animation::offline::RawFloatTrack* ) override {
            return false;
        }
        bool Import( const char*, const char*, const char*, NodeProperty::Type, float,
                     ozz::animation::offline::RawFloat2Track* ) override {
            return false;
        }
        bool Import( const char*, const char*, const char*, NodeProperty::Type, float,
                     ozz::animation::offline::RawFloat3Track* ) override {
            return false;
        }
        bool Import( const char*, const char*, const char*, NodeProperty::Type, float,
                     ozz::animation::offline::RawFloat4Track* ) override {
            return false;
        }

        tinygltf::TinyGLTF m_loader;
        tinygltf::Model m_model;
    };

    static auto ComponentSize( Int32 componentType ) -> Size {
        switch ( componentType ) {
            case TINYGLTF_COMPONENT_TYPE_FLOAT:
                return sizeof( float );
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                return sizeof( UInt32 );
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                return sizeof( UInt16 );
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                return sizeof( UInt8 );
            default:
                return 0;
        }
    }

    static auto TypeCount( Int32 type ) -> Size {
        switch ( type ) {
            case TINYGLTF_TYPE_SCALAR:
                return 1;
            case TINYGLTF_TYPE_VEC2:
                return 2;
            case TINYGLTF_TYPE_VEC3:
                return 3;
            case TINYGLTF_TYPE_VEC4:
                return 4;
            default:
                return 1;
        }
    }

    static auto GetMikotoWrapMode( Int32 wrapMode ) -> SamplerWrapMode {
        switch ( wrapMode ) {
            case -1:
            case 10497:
                return SamplerWrapMode::WRAP_REPEAT;
            case 33071:
                return SamplerWrapMode::WRAP_CLAMP_TO_EDGE;
            case 33648:
                return SamplerWrapMode::MIRRORED_REPEAT;
        }

        return SamplerWrapMode::WRAP_REPEAT;
    }

    static auto GetMikotoFilterMode( Int32 filterMode ) -> SamplerFilter {
        switch ( filterMode ) {
            case -1:
            case 9728:
                return SamplerFilter::FILTER_NEAREST;
            case 9729:
                return SamplerFilter::FILTER_LINEAR;
            case 9984:
                return SamplerFilter::FILTER_NEAREST;
            case 9985:
                return SamplerFilter::FILTER_NEAREST;
            case 9986:
                return SamplerFilter::FILTER_LINEAR;
            case 9987:
                return SamplerFilter::FILTER_LINEAR;
        }

        return SamplerFilter::FILTER_NEAREST;
    }

    static auto ToMat4F( const tinygltf::Node& node ) -> Mat4F {
        glm::mat4 transform{ 1.0f };

        if ( !node.matrix.empty() ) {
            const double* m = node.matrix.data();

            transform = glm::mat4(
                    ( float )m[0], ( float )m[1], ( float )m[2], ( float )m[3],
                    ( float )m[4], ( float )m[5], ( float )m[6], ( float )m[7],
                    ( float )m[8], ( float )m[9], ( float )m[10], ( float )m[11],
                    ( float )m[12], ( float )m[13], ( float )m[14], ( float )m[15] );
        }

        return transform;
    }

    static Mat4F ComputeNodeTransform( const tinygltf::Node& node ) {
        Mat4F transform{ 1.0f };

        if ( !node.matrix.empty() ) {
            transform = ToMat4F( node );
        } else {
            Vec3F translation{ 0.0f };
            Vec3F scale{ 1.0f };
            Quat rotation{ 1, 0, 0, 0 };

            if ( !node.translation.empty() )
                translation = Vec3F(
                        node.translation[0],
                        node.translation[1],
                        node.translation[2] );

            if ( !node.scale.empty() )
                scale = Vec3F(
                        node.scale[0],
                        node.scale[1],
                        node.scale[2] );

            if ( !node.rotation.empty() )
                rotation = Quat(
                        node.rotation[3],
                        node.rotation[0],
                        node.rotation[1],
                        node.rotation[2] );

            transform =
                    glm::translate( Mat4F{ 1.0f }, translation ) *
                    glm::mat4_cast( rotation ) *
                    glm::scale( Mat4F{ 1.0f }, scale );
        }

        return transform;
    }

    static auto ReadAccessorAsFloat(
            const tinygltf::Model& model,
            const tinygltf::Accessor& accessor ) -> std::vector<float> {
        const auto& view{ model.bufferViews[accessor.bufferView] };
        const auto& buffer{ model.buffers[view.buffer] };

        const auto compSize{ ComponentSize( accessor.componentType ) };
        const auto elemSize{ TypeCount( accessor.type ) };
        const auto stride{ accessor.ByteStride( view ) };

        const auto* dataPtr{ buffer.data.data() + view.byteOffset + accessor.byteOffset };

        std::vector<float> result{};
        result.resize( accessor.count * elemSize );

        for ( Size i{}; i < accessor.count; ++i ) {
            const auto* element = dataPtr + i * ( stride ? stride : compSize * elemSize );

            for ( Size c{}; c < elemSize; ++c ) {
                const auto* compPtr = element + c * compSize;
                float value{};

                switch ( accessor.componentType ) {
                    case TINYGLTF_COMPONENT_TYPE_FLOAT:
                        value = *reinterpret_cast<const float*>( compPtr );
                        break;

                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                        auto v = *reinterpret_cast<const UInt8*>( compPtr );
                        value = accessor.normalized ? v / 255.f : static_cast<float>( v );
                        break;
                    }

                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                        auto v = *reinterpret_cast<const UInt16*>( compPtr );
                        value = accessor.normalized ? v / 65535.f : static_cast<float>( v );
                        break;
                    }

                    default:
                        value = 0.f;
                        break;
                }

                result[i * elemSize + c] = value;
            }
        }

        return result;
    }

    template<typename TVec>
    static auto LoadVertexAttribute(
            const tinygltf::Model& model,
            const tinygltf::Primitive& primitive,
            const std::string& attributeName,
            std::vector<VertexData>& vertices,
            TVec VertexData::* member,
            Size componentCount ) -> void {

        if ( !primitive.attributes.contains( attributeName ) ) {
            return;
        }

        const auto& accessor{ model.accessors[primitive.attributes.at( attributeName )] };
        auto data{ ReadAccessorAsFloat( model, accessor ) };

        const Size vertexCount{ vertices.size() };

        for ( Size i{}; i < vertexCount; ++i ) {
            TVec value{};

            if constexpr ( std::is_same_v<TVec, Vec2F> ) {
                value = {
                    data[i * componentCount + 0],
                    data[i * componentCount + 1]
                };
            } else if constexpr ( std::is_same_v<TVec, Vec3F> ) {
                value = {
                    data[i * componentCount + 0],
                    data[i * componentCount + 1],
                    data[i * componentCount + 2]
                };
            } else if constexpr ( std::is_same_v<TVec, Vec4F> ) {
                value = {
                    data[i * componentCount + 0],
                    data[i * componentCount + 1],
                    data[i * componentCount + 2],
                    data[i * componentCount + 3]
                };
            }

            vertices[i].*member = value;
        }
    }

    GLTFImporter::GLTFImporter( GpuDevice* device )
        : ModelImporter{ device } {
        for ( Int32 count{}; count < ThreadUtils::InferConcurrentThreads(); ++count ) {
            m_Importers.emplace_back( CreateScope<LoaderData>( count ) );
        }
    }

    auto GLTFImporter::Import( const ModelLoadDescription& description, ModelData& out ) -> void {
        auto iter{ m_Importers.end() };
        do {
            iter = TryAcquireImporter();
        } while ( iter == m_Importers.end() );

        MKT_CORE_LOGGER_DEBUG( "Using GLTF importer {}", ( *iter )->Index );

        Import( *( *iter ), description, out );
        ( *iter )->IsFree.store( true, std::memory_order_release );
    }

    auto GLTFImporter::LoadPrimitives( tinygltf::Model& model, ModelData& modelData ) -> void {
        for ( const auto& mesh: model.meshes ) {
            for ( const auto& primitive: mesh.primitives ) {
                MeshNodeData node{};
                node.Name = mesh.name;
                node.MaterialIndex = primitive.material;

                const auto& posAccessor{ model.accessors[primitive.attributes.at( "POSITION" )] };

                const Size vertexCount{ posAccessor.count };
                node.Vertices.resize( vertexCount );

                // POSITION (required)
                LoadVertexAttribute(
                        model,
                        primitive,
                        "POSITION",
                        node.Vertices,
                        &VertexData::Position,
                        3 );

                // NORMAL
                LoadVertexAttribute(
                        model,
                        primitive,
                        "NORMAL",
                        node.Vertices,
                        &VertexData::Normals,
                        3 );

                // COLOR_0 (can be VEC3 or VEC4)
                if ( primitive.attributes.contains( "COLOR_0" ) ) {
                    const auto& accessor =
                            model.accessors[primitive.attributes.at( "COLOR_0" )];

                    const auto compCount = TypeCount( accessor.type );

                    LoadVertexAttribute(
                            model,
                            primitive,
                            "COLOR_0",
                            node.Vertices,
                            &VertexData::Colors,
                            compCount == 4 ? 4 : 3 );
                }

                // TEXCOORD_0
                LoadVertexAttribute(
                        model,
                        primitive,
                        "TEXCOORD_0",
                        node.Vertices,
                        &VertexData::UV_0,
                        2 );

                // TEXCOORD_1
                LoadVertexAttribute(
                        model,
                        primitive,
                        "TEXCOORD_1",
                        node.Vertices,
                        &VertexData::UV_1,
                        2 );

                // JOINTS_0
                LoadVertexAttribute(
                        model,
                        primitive,
                        "JOINTS_0",
                        node.Vertices,
                        &VertexData::Joints,
                        4 );

                // WEIGHTS_0
                LoadVertexAttribute(
                        model,
                        primitive,
                        "WEIGHTS_0",
                        node.Vertices,
                        &VertexData::Weights,
                        4 );

                if ( primitive.indices >= 0 ) {
                    const auto& accessor{ model.accessors[primitive.indices] };
                    const auto& view{ model.bufferViews[accessor.bufferView] };
                    const auto& buffer{ model.buffers[view.buffer] };
                    const auto* dataPtr{ buffer.data.data() + view.byteOffset + accessor.byteOffset };

                    node.Indices.resize( accessor.count );

                    for ( Size i{}; i < accessor.count; ++i ) {
                        if ( accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT ) {
                            node.Indices[i] = reinterpret_cast<const UInt16*>( dataPtr )[i];
                        } else if ( accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT ) {
                            node.Indices[i] = reinterpret_cast<const UInt32*>( dataPtr )[i];
                        }
                    }
                }

                modelData.MeshNodes.push_back( std::move( node ) );
            }
        }
    }

    auto GLTFImporter::LoadMaterials( tinygltf::Model& model, ModelData& modelData, const std::string& rootPath ) -> void {
        modelData.Materials.reserve( model.materials.size() );

        // Default format is SRGB for emissive and base color
        // rest is unrorm rgba8
        TextureLoadDescription loadInfo{};
        loadInfo.WithType( TextureType::TEXTURE_2D );

        for ( const auto& gltfMaterial: model.materials ) {
            MaterialProperties props{};
            props.Name = gltfMaterial.name;
            props.IsDoubleSided = gltfMaterial.doubleSided;

            const auto& pbr{ gltfMaterial.pbrMetallicRoughness };

            props.BaseColorFactor = {
                static_cast<float>( pbr.baseColorFactor[0] ),
                static_cast<float>( pbr.baseColorFactor[1] ),
                static_cast<float>( pbr.baseColorFactor[2] ),
                static_cast<float>( pbr.baseColorFactor[3] )
            };

            props.MetallicFactor = static_cast<float>( pbr.metallicFactor );
            props.RoughnessFactor = static_cast<float>( pbr.roughnessFactor );

            // TODO(kate): check notes, these textures need specific format not any random format
            // iirc base, diffuse need srgb; ao, metallic, etc need linear??
            // Base color
            if ( pbr.baseColorTexture.index >= 0 ) {
                const auto& tex{ model.textures[pbr.baseColorTexture.index] };
                props.BaseColorTextureSet = pbr.baseColorTexture.texCoord;

                loadInfo.WithMapType( MapType::BASE_COLOR_TEXTURE );
                loadInfo
                        .WithFile( FileService::Get()->LoadFile( Path{ PathBuilder()
                                                                               .WithPath( rootPath )
                                                                               .WithPath( model.images[tex.source].uri )
                                                                               .Build() } ) )
                        .WithFormat( TextureFormat::RGBA8_UNORM );

                TextureHandle texture{ AssetsService::Get()->LoadAsset<Texture>( loadInfo ) };
                if ( !texture.IsEmpty() ) {
                    props.TexturesByUri[loadInfo.TextureFile->GetPath()] = texture;
                }
            }

            // Metall roughness
            if ( pbr.metallicRoughnessTexture.index >= 0 ) {
                const auto& tex{ model.textures[pbr.metallicRoughnessTexture.index] };
                props.MetallicRoughnessTextureSet = pbr.metallicRoughnessTexture.texCoord;

                loadInfo.WithMapType( MapType::METALLIC_ROUGHNESS_TEXTURE );
                loadInfo.WithFile( FileService::Get()->LoadFile( Path{ PathBuilder()
                                                                               .WithPath( rootPath )
                                                                               .WithPath( model.images[tex.source].uri )
                                                                               .Build() } ) )
                        .WithFormat( TextureFormat::RGBA8_UNORM );

                TextureHandle texture{ AssetsService::Get()->LoadAsset<Texture>( loadInfo ) };
                if ( !texture.IsEmpty() ) {
                    props.TexturesByUri[loadInfo.TextureFile->GetPath()] = texture;
                }
            }

            props.RoughnessFactor = pbr.roughnessFactor;
            props.MetallicFactor = static_cast<float>( gltfMaterial.pbrMetallicRoughness.metallicFactor );

            // Normal
            if ( gltfMaterial.normalTexture.index >= 0 ) {
                const auto& tex{ model.textures[gltfMaterial.normalTexture.index] };
                props.NormalTextureSet = gltfMaterial.normalTexture.texCoord;
                props.NormalScale = static_cast<float>( gltfMaterial.normalTexture.scale );

                loadInfo.WithMapType( MapType::NORMAL_TEXTURE );
                loadInfo.WithFile( FileService::Get()->LoadFile( Path{ PathBuilder()
                                                                               .WithPath( rootPath )
                                                                               .WithPath( model.images[tex.source].uri )
                                                                               .Build() } ) )
                        .WithFormat( TextureFormat::RGBA8_UNORM );

                TextureHandle texture{ AssetsService::Get()->LoadAsset<Texture>( loadInfo ) };
                if ( !texture.IsEmpty() ) {
                    props.TexturesByUri[loadInfo.TextureFile->GetPath()] = texture;
                }
            }

            // Occlusion
            if ( gltfMaterial.occlusionTexture.index >= 0 ) {
                const auto& tex{ model.textures[gltfMaterial.occlusionTexture.index] };
                props.OcclusionTextureSet = gltfMaterial.occlusionTexture.texCoord;
                props.OcclusionStrength = static_cast<float>( gltfMaterial.occlusionTexture.strength );

                loadInfo.WithMapType( MapType::AMBIENT_OCCLUSION_TEXTURE );
                loadInfo.WithFile( FileService::Get()->LoadFile( Path{ PathBuilder()
                                                                               .WithPath( rootPath )
                                                                               .WithPath( model.images[tex.source].uri )
                                                                               .Build() } ) )

                        .WithFormat( TextureFormat::RGBA8_UNORM );

                TextureHandle texture{ AssetsService::Get()->LoadAsset<Texture>( loadInfo ) };
                if ( !texture.IsEmpty() ) {
                    props.TexturesByUri[loadInfo.TextureFile->GetPath()] = texture;
                }
            }

            // Emissive
            if ( gltfMaterial.emissiveTexture.index >= 0 ) {
                const auto& tex{ model.textures[gltfMaterial.emissiveTexture.index] };
                props.EmissiveTextureSet = gltfMaterial.emissiveTexture.texCoord;

                loadInfo.WithMapType( MapType::EMISSIVE_TEXTURE );
                loadInfo.WithFile( FileService::Get()->LoadFile( Path{ PathBuilder()
                                                                               .WithPath( rootPath )
                                                                               .WithPath( model.images[tex.source].uri )
                                                                               .Build() } ) )
                        .WithFormat( TextureFormat::RGBA8_UNORM );

                TextureHandle texture{ AssetsService::Get()->LoadAsset<Texture>( loadInfo ) };
                if ( !texture.IsEmpty() ) {
                    props.TexturesByUri[loadInfo.TextureFile->GetPath()] = texture;
                }
            }

            props.EmissiveFactor = {
                static_cast<float>( gltfMaterial.emissiveFactor[0] ),
                static_cast<float>( gltfMaterial.emissiveFactor[1] ),
                static_cast<float>( gltfMaterial.emissiveFactor[2] )
            };

            // Alpha (Default is Opaque unless otherwise specified)
            if ( gltfMaterial.alphaMode == "BLEND" ) {
                props.AlphaMask = PBR_AlphaMode::Blend;
            } else if ( gltfMaterial.alphaMode == "MASK" ) {
                props.AlphaMask = PBR_AlphaMode::Mask;
                props.AlphaMaskCutoff = 0.5f;
            }

            props.AlphaMaskCutoff = static_cast<float>( gltfMaterial.alphaCutoff );

            // Extensions
            auto ext{ gltfMaterial.extensions.find( KHR_PBR_SpecularGlossiness.data() ) };
            if ( gltfMaterial.extensions.find( KHR_PBR_SpecularGlossiness.data() ) != gltfMaterial.extensions.end() ) {

                if ( ext->second.Has( "specularGlossinessTexture" ) ) {
                    auto index{ ext->second.Get( "specularGlossinessTexture" ).Get( "index" ) };

                    auto texIndex = index.Get<int>();
                    auto texCoordSet = ext->second.Get( "specularGlossinessTexture" ).Get( "texCoord" ).Get<int>();

                    loadInfo.WithMapType( MapType::SPECULAR_GLOSSINESS );
                    loadInfo.WithFile( FileService::Get()->LoadFile(
                            Path{ PathBuilder()
                                          .WithPath( rootPath )
                                          .WithPath( model.images[texIndex].uri )
                                          .Build() } ) );

                    TextureHandle texture{ AssetsService::Get()->LoadAsset<Texture>( loadInfo ) };
                    if ( !texture.IsEmpty() ) {
                        props.TexturesByUri[loadInfo.TextureFile->GetPath()] = texture;
                    }

                    props.SpecularGlossinessSet = texCoordSet;
                    props.Workflow = PBR_Workflow::SpecularGlossiness;
                }

                if ( ext->second.Has( "diffuseTexture" ) ) {
                    auto index{ ext->second.Get( "diffuseTexture" ).Get( "index" ) };
                    loadInfo.WithMapType( MapType::DIFFUSE_TEXTURE );
                    loadInfo.WithFile( FileService::Get()->LoadFile( Path{ PathBuilder()
                                                                                   .WithPath( rootPath )
                                                                                   .WithPath( model.images[index.Get<int>()].uri )
                                                                                   .Build() } ) )
                            .WithFormat( TextureFormat::RGBA8_UNORM );

                    TextureHandle texture{ AssetsService::Get()->LoadAsset<Texture>( loadInfo ) };
                    if ( !texture.IsEmpty() ) {
                        props.TexturesByUri[loadInfo.TextureFile->GetPath()] = texture;
                    }
                }

                if ( ext->second.Has( "diffuseFactor" ) ) {
                    auto factor{ ext->second.Get( "diffuseFactor" ) };
                    for ( UInt32 i{}; i < factor.ArrayLen(); i++ ) {
                        auto val{ factor.Get( i ) };
                        //material.extension.diffuseFactor[i] = val.IsNumber() ? ( float )val.Get<double>() : ( float )val.Get<int>();
                    }
                }

                if ( ext->second.Has( "specularFactor" ) ) {
                    auto factor{ ext->second.Get( "specularFactor" ) };
                    for ( UInt32 i{}; i < factor.ArrayLen(); i++ ) {
                        auto val{ factor.Get( i ) };
                        //material.extension.specularFactor[i] = val.IsNumber() ? ( float )val.Get<double>() : ( float )val.Get<int>();
                    }
                }
            }

            if ( gltfMaterial.extensions.find( KHR_PBR_Unlit.data() ) != gltfMaterial.extensions.end() ) {
                props.Unlit = true;
            }

            if ( gltfMaterial.extensions.find( KHR_Emissive_Strength.data() ) != gltfMaterial.extensions.end() ) {
                auto ext = gltfMaterial.extensions.find( KHR_Emissive_Strength.data() );
                if ( ext->second.Has( "emissiveStrength" ) ) {
                    auto value{ ext->second.Get( "emissiveStrength" ) };
                    props.EmissiveStrength = ( float )value.Get<double>();
                }
            }

            modelData.Materials.push_back( std::move( props ) );
        }
    }

    auto GLTFImporter::Import( LoaderData& loaderData, const ModelLoadDescription& description, ModelData& out ) -> void {
        tinygltf::Model model{};

        bool res{ loaderData.Loader.LoadASCIIFromFile( &model, &loaderData.Err, &loaderData.Warn, description.ModelFile->GetPath() ) };
        if ( !loaderData.Warn.empty() ) {
            MKT_CORE_LOGGER_WARN( "GLTF Loader WARN: {}", loaderData.Warn );
        }

        if ( !loaderData.Err.empty() ) {
            MKT_CORE_LOGGER_ERROR( "GLTF Loader ERROR: {}", loaderData.Err );
        }

        if ( !res ) {
            MKT_CORE_LOGGER_ERROR( "Failed to load glTF: {}", description.ModelFile->GetPath() );
        } else {
            MKT_CORE_LOGGER_DEBUG( "Loaded glTF: {}", description.ModelFile->GetPath() );

            // Reference root path for loading textures
            const std::string rootPath{ Filesystem::StripFileName( description.ModelFile->GetPath() ) };

            LoadPrimitives( model, out );
            LoadMaterials( model, out, rootPath );

            // Build animations and skeleton
            // We will save the loaded ozz files in disk along withh a mikoto metadata file
            // if the file has been processed we simply load the ozz animation files, otherwise we
            // create the necessary resources (cleaqr model here because importer loads its own)

            if (!model.animations.empty()) {
                GltfAnimImporter animationImporter{};
                SkinningBuilder builder{ description.ModelFile->GetPath() };

                if ( builder.Build( animationImporter ) ) {
                    // Get the skeleton and animations
                    builder.FillModelData( out );
                }
            }
        }
    }
}// namespace Mikoto