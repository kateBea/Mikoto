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

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

#include <glm/gtc/quaternion.hpp>

#include <Renderer/Clouds/CloudDensityBaker.hh>

namespace mikoto::renderer {

    auto CloudDensityBaker::CreateCumulusSkyRecipe() -> CloudAuthoringDescription {
        CloudAuthoringDescription recipe{};
        recipe.mDimensions = { 256, 64, 256 };
        recipe.mDomainAspect = { 13.0f, 1.8f, 13.0f };
        recipe.mProfileDepth = .035f;
        // Three simulated sources are reused over an irregular, broken layout.
        // Dense bases and tall lobes come from the fluid snapshot, not coverage noise.
        for ( core::u32 z{}; z < 4; ++z ) {
            for ( core::u32 x{}; x < 4; ++x ) {
                const core::u32 i{ x + z * 4 };
                CloudAuthoringLayer layer{};
                layer.mFluid.mResolution = 64;
                layer.mFluid.mSteps = 64;
                layer.mFluid.mSeed = 17 + ( i % 3 ) * 74;
                layer.mFluid.mBuoyancy = 1.1f;
                layer.mFluid.mVorticity = 2.1f;
                layer.mFluid.mHumidity = .85f;
                layer.mFluid.mCooling = 1.2f;
                layer.mFluid.mWindShear = .6f;
                layer.mFluid.mDetailType = .82f;
                layer.mFluid.mCloudForm = CloudForm::eCumulus;
                layer.mOffset = { ( core::f32( x ) - 1.5f ) * .23f + ( z == 1 ? .04f : -.02f ),
                    -.04f + core::f32( i % 3 ) * .04f, ( core::f32( z ) - 1.5f ) * .23f + ( x == 1 ? -.04f : .02f ) };
                layer.mScale = { .24f + core::f32( i % 3 ) * .025f, .9f, .27f };
                layer.mRotation.y = core::f32( i * 47 );
                layer.mErosion = .12f;
                recipe.mLayers.push_back( layer );
            }
        }
        return recipe;
    }


    auto CloudDensityBaker::Compose( const CloudAuthoringDescription &description,
        const eastl::vector<CloudLayerGrid> &grids ) -> CloudDensityBake {
        using namespace core;
        const uint3 size{ description.mDimensions == uint3{} ? uint3{ description.mResolution } : description.mDimensions };
        const usize count{ usize( size.x ) * size.y * size.z };
        eastl::vector<f32> density( count, 0 ), detail( count, .75f );
        eastl::vector<CloudDensityBakeDescription> cachedDescriptions{};
        eastl::vector<CloudLayerGrid> cachedFluids{};
        for ( usize layerIndex{}; layerIndex < description.mLayers.size(); ++layerIndex ) {
            const auto &layer{ description.mLayers[layerIndex] };
            if ( !layer.mEnabled ) {
                continue;
            }
            const CloudLayerGrid *source{ &grids[layerIndex] };
            if ( layer.mSource == CloudLayerSource::eFluid ) {
                usize found{};
                while ( found < cachedDescriptions.size() && !( cachedDescriptions[found] == layer.mFluid ) ) {
                    ++found;
                }
                if ( found == cachedDescriptions.size() ) {
                    auto baked{ Bake( layer.mFluid ) };
                    cachedDescriptions.push_back( layer.mFluid );
                    cachedFluids.push_back( { baked.mDimensions, eastl::move( baked.mVoxels ) } );
                }
                source = &cachedFluids[found];
            }
            const bool grid{ layer.mSource == CloudLayerSource::eFluid || layer.mSource == CloudLayerSource::eGrid ||
                             layer.mSource == CloudLayerSource::eMesh || layer.mSource == CloudLayerSource::ePointCloud };
            if ( grid ) {
                for ( const auto &voxel: source->mVoxels ) {
                }
            }
            const auto inverseRotation{ glm::mat3_cast( glm::conjugate( glm::quat{ glm::radians( layer.mRotation ) } ) ) };
            for ( u32 z{}; z < size.z; ++z ) {
                for ( u32 y{}; y < size.y; ++y ) {
                    for ( u32 x{}; x < size.x; ++x ) {
                        const usize i{ x + usize( size.x ) * ( y + usize( size.y ) * z ) };
                        const auto uv{ inverseRotation * ( ( float3{ x, y, z } + .5f ) / float3{ size } - .5f - layer.mOffset ) / layer.mScale + .5f };
                        f32 value{};
                        f32 type{ layer.mFluid.mDetailType };
                        if ( glm::all( glm::greaterThanEqual( uv, float3{ 0 } ) ) && glm::all( glm::lessThanEqual( uv, float3{ 1 } ) ) ) {
                            if ( grid ) {
                                const float3 p{ glm::clamp( uv * float3{ source->mDimensions } - .5f, float3{}, float3{ source->mDimensions - 1u } ) };
                                const uint3 lo{ p };
                                const uint3 hi{ glm::min( lo + 1u, source->mDimensions - 1u ) };
                                const float3 f{ glm::fract( p ) };
                                float3 sample{};
                                for ( u32 dz{}; dz < 2; ++dz ) {
                                    for ( u32 dy{}; dy < 2; ++dy ) {
                                        for ( u32 dx{}; dx < 2; ++dx ) {
                                            const uint3 q{ dx ? hi.x : lo.x, dy ? hi.y : lo.y, dz ? hi.z : lo.z };
                                            const auto &v{ source->mVoxels[q.x + usize( source->mDimensions.x ) * ( q.y + usize( source->mDimensions.y ) * q.z )] };
                                            const float3 w{ glm::mix( 1.0f - f, f, float3{ dx, dy, dz } ) };
                                            sample += float3{ v.mProfile, v.mDetailType, v.mDensityScale } * w.x * w.y * w.z;
                                        }
                                    }
                                }
                                value = sample.x * sample.z;
                                type = sample.y;
                            }
                            else {
                                const float3 p{ glm::abs( uv - .5f ) * 2.0f };
                                const f32 distance{ layer.mSource == CloudLayerSource::eEllipsoid ? glm::length( p ) : glm::max( p.x, glm::max( p.y, p.z ) ) };
                                value = 1 - glm::smoothstep( 1.0f - layer.mSoftness, 1.0f, distance );
                            }
                        }
                        value = glm::clamp( ( value - layer.mErosion ) / glm::max( 1.0f - layer.mErosion, .001f ), 0.0f, 1.0f ) * layer.mOpacity;
                        switch ( layer.mOperation ) {
                            case CloudLayerOperation::eUnion:
                                if ( value > density[i] ) {
                                    detail[i] = type;
                                }
                                density[i] = glm::max( density[i], value );
                                break;
                            case CloudLayerOperation::eSubtract:
                                density[i] *= 1 - value;
                                break;
                            case CloudLayerOperation::eIntersect:
                                density[i] = glm::min( density[i], value );
                                break;
                            case CloudLayerOperation::eReplace:
                                density[i] = value;
                                detail[i] = type;
                                break;
                        }
                    }
                }
            }
        }
        auto voxels{ BuildVolume( size, density, .75f, description.mProfileDepth, description.mDomainAspect ) };
        for ( usize i{}; i < count; ++i ) {
            if ( voxels[i].mDensityScale > 0 ) {
                voxels[i].mDetailType = glm::clamp( detail[i], 0.0f, 1.0f );
            }
        }
        return { uint3{ size }, eastl::move( density ), CloudForm::eCumulus, eastl::move( voxels ) };
    }

    auto CloudDensityBaker::Voxelize( const eastl::vector<core::float3> &positions,
        const eastl::vector<core::u32> &indices, core::uint3 dimensions, core::f32 pointRadius,
        core::f32 detailType, core::f32 profileDepth ) -> CloudLayerGrid {
        using namespace core;
        const u64 count{ u64( dimensions.x ) * dimensions.y * dimensions.z };
        float3 lower{ std::numeric_limits<f32>::max() };
        float3 upper{ -std::numeric_limits<f32>::max() };
        for ( const auto &p: positions ) {
            lower = glm::min( lower, p );
            upper = glm::max( upper, p );
        }
        const auto extent{ upper - lower };
        const f32 scale{ glm::max( glm::max( extent.x, glm::max( extent.y, extent.z ) ), .001f ) };
        const auto center{ lower + extent * .5f };
        eastl::vector<float3> vertices{};
        vertices.reserve( positions.size() );
        for ( const auto &point: positions ) {
            vertices.push_back( ( point - center ) / scale * .9f + .5f );
        }
        eastl::vector<f32> density( static_cast<usize>( count ), 0 );
        const auto index{ [dimensions]( uint3 p ) {
            return usize( p.x ) + usize( dimensions.x ) * ( p.y + usize( dimensions.y ) * p.z );
        } };

        if ( indices.empty() ) {
            // Sparse point splats only visit their affected voxel bounds.
            for ( const auto &point: vertices ) {
                const uint3 lo{ glm::clamp( glm::floor( ( point - pointRadius ) * float3{ dimensions } ), float3{}, float3{ dimensions - 1u } ) };
                const uint3 hi{ glm::clamp( glm::ceil( ( point + pointRadius ) * float3{ dimensions } ), float3{}, float3{ dimensions - 1u } ) };
                for ( u32 z{ lo.z }; z <= hi.z; ++z ) {
                    for ( u32 y{ lo.y }; y <= hi.y; ++y ) {
                        for ( u32 x{ lo.x }; x <= hi.x; ++x ) {
                            const uint3 p{ x, y, z };
                            const f32 distance{ glm::length( ( float3{ p } + .5f ) / float3{ dimensions } - point ) };
                            density[index( p )] = glm::max( density[index( p )], 1 - glm::smoothstep( pointRadius * .6f, pointRadius, distance ) );
                        }
                    }
                }
            }
        }
        else {
            struct Triangle {
                float3 mA{};
                float3 mB{};
                float3 mC{};
                float3 mCenter{};
            };
            struct Node {
                float3 mLower{};
                float3 mUpper{};
                u32 mBegin{};
                u32 mCount{};
                u32 mLeft{};
                u32 mRight{};
            };
            eastl::vector<Triangle> triangles{};
            triangles.reserve( indices.size() / 3 );
            for ( usize i{}; i < indices.size(); i += 3 ) {
                const auto a{ vertices[indices[i]] };
                const auto b{ vertices[indices[i + 1]] };
                const auto c{ vertices[indices[i + 2]] };
                triangles.push_back( Triangle{
                    .mA = a,
                    .mB = b,
                    .mC = c,
                    .mCenter = ( a + b + c ) / 3.0f,
                } );
            }
            eastl::vector<Node> nodes{};
            const auto build{ [&]( auto &&self, u32 begin, u32 length ) -> u32 {
                const u32 id{ static_cast<u32>( nodes.size() ) };
                Node node{
                    .mLower = float3{ 1e30f },
                    .mUpper = float3{ -1e30f },
                    .mBegin = begin,
                    .mCount = length,
                };
                for ( u32 i{ begin }; i < begin + length; ++i ) {
                    const auto &t{ triangles[i] };
                    node.mLower = glm::min( node.mLower, glm::min( t.mA, glm::min( t.mB, t.mC ) ) );
                    node.mUpper = glm::max( node.mUpper, glm::max( t.mA, glm::max( t.mB, t.mC ) ) );
                }
                nodes.push_back( node );
                if ( length > 8 ) {
                    const auto extent{ node.mUpper - node.mLower };
                    const u32 axis{ extent.x > extent.y ? ( extent.x > extent.z ? 0u : 2u ) : ( extent.y > extent.z ? 1u : 2u ) };
                    const u32 middle{ begin + length / 2 };
                    std::nth_element( triangles.begin() + begin, triangles.begin() + middle, triangles.begin() + begin + length,
                        [axis]( const Triangle &a, const Triangle &b ) {
                            return a.mCenter[axis] < b.mCenter[axis];
                        } );
                    nodes[id].mLeft = self( self, begin, middle - begin );
                    nodes[id].mRight = self( self, middle, begin + length - middle );
                    nodes[id].mCount = 0;
                }
                return id;
            } };
            build( build, 0, static_cast<u32>( triangles.size() ) );
            const float3 direction{ glm::normalize( float3{ 1.0f, .000371f, .000719f } ) };
            const auto inverse{ 1.0f / direction };
            for ( u32 z{}; z < dimensions.z; ++z ) {
                for ( u32 y{}; y < dimensions.y; ++y ) {
                    for ( u32 x{}; x < dimensions.x; ++x ) {
                        const uint3 p{ x, y, z };
                        const float3 origin{ ( float3{ p } + .5f ) / float3{ dimensions } };
                        u32 stack[64]{};
                        u32 top{ 1 };
                        u32 hits{};
                        while ( top > 0 ) {
                            const auto &node{ nodes[stack[--top]] };
                            const auto a{ ( node.mLower - origin ) * inverse };
                            const auto b{ ( node.mUpper - origin ) * inverse };
                            const auto lo{ glm::min( a, b ) };
                            const auto hi{ glm::max( a, b ) };
                            if ( glm::min( hi.x, glm::min( hi.y, hi.z ) ) < glm::max( 0.0f, glm::max( lo.x, glm::max( lo.y, lo.z ) ) ) ) {
                                continue;
                            }
                            if ( node.mCount == 0 ) {
                                stack[top++] = node.mLeft;
                                stack[top++] = node.mRight;
                                continue;
                            }
                            for ( u32 i{ node.mBegin }; i < node.mBegin + node.mCount; ++i ) {
                                const auto &t{ triangles[i] };
                                const auto e1{ t.mB - t.mA };
                                const auto e2{ t.mC - t.mA };
                                const auto h{ glm::cross( direction, e2 ) };
                                const f32 determinant{ glm::dot( e1, h ) };
                                if ( glm::abs( determinant ) < 1e-10f ) {
                                    continue;
                                }
                                const auto v{ origin - t.mA };
                                const auto q{ glm::cross( v, e1 ) };
                                const f32 u{ glm::dot( v, h ) / determinant };
                                const f32 w{ glm::dot( direction, q ) / determinant };
                                hits += u >= 0 && w >= 0 && u + w <= 1 && glm::dot( e2, q ) / determinant > 1e-7f;
                            }
                        }
                        density[index( p )] = ( hits & 1u ) ? 1.0f : 0.0f;
                    }
                }
            }
        }
        return { dimensions, BuildVolume( dimensions, density, detailType, profileDepth ) };
    }

    auto CloudDensityBaker::BuildVolume( core::uint3 dimensions, const eastl::vector<core::f32> &density,
        core::f32 detailType, core::f32 profileDepth, core::float3 aspect ) -> eastl::vector<CloudVoxel> {
        using namespace core;

        const u64 count{ u64( dimensions.x ) * dimensions.y * dimensions.z };

        aspect /= glm::min( aspect.x, glm::min( aspect.y, aspect.z ) );

        // Exact separable squared Euclidean distance to empty cells. The lower
        // envelope of parabolas keeps each line linear in its voxel count.
        constexpr f32 infinity{ 1e20f };
        eastl::vector<f32> distance( density.size() );
        for ( usize i{}; i < density.size(); ++i ) {
            distance[i] = density[i] > 0 ? infinity : 0;
        }

        const auto index{ [dimensions]( uint3 p ) -> usize {
            return usize( p.x ) + usize( dimensions.x ) * ( usize( p.y ) + usize( dimensions.y ) * p.z );
        } };
        for ( u32 axis{}; axis < 3; ++axis ) {
            const u32 length{ dimensions[axis] };
            const f32 spacing{ aspect[axis] / length };
            const f32 squaredSpacing{ spacing * spacing };
            for ( u64 line{}; line < count / length; ++line ) {
                uint3 p{ axis == 0 ? uint3{ 0, line % dimensions.y, line / dimensions.y } : axis == 1 ? uint3{ line % dimensions.x, 0, line / dimensions.x }
                                                                                                      : uint3{ line % dimensions.x, line / dimensions.x, 0 } };
                f32 values[256]{};
                f32 boundaries[257]{};
                i32 features[256]{};
                i32 last{ -1 };
                for ( u32 sample{}; sample < length; ++sample ) {
                    p[axis] = sample;
                    values[sample] = distance[index( p )];
                    if ( values[sample] >= infinity ) {
                        continue;
                    }
                    f32 crossing{ -infinity };
                    while ( last >= 0 ) {
                        const i32 previous{ features[last] };
                        crossing = ( values[sample] - values[previous] + squaredSpacing * ( f32( sample ) * sample - f32( previous ) * previous ) ) /
                                   ( 2 * squaredSpacing * ( f32( sample ) - previous ) );
                        if ( crossing > boundaries[last] ) {
                            break;
                        }
                        --last;
                    }
                    features[++last] = as<i32>( sample );
                    boundaries[last] = last == 0 ? -infinity : crossing;
                    boundaries[last + 1] = infinity;
                }
                i32 selected{};
                for ( u32 sample{}; sample < length; ++sample ) {
                    p[axis] = sample;
                    while ( selected < last && boundaries[selected + 1] < sample ) {
                        ++selected;
                    }
                    const f32 delta{ last < 0 ? 0 : f32( sample ) - features[selected] };
                    distance[index( p )] = last < 0 ? infinity : values[features[selected]] + delta * delta * squaredSpacing;
                }
            }
        }

        eastl::vector<CloudVoxel> voxels( density.size() );
        const float3 spacing{ aspect / float3{ dimensions } };
        const f32 halfVoxel{ .5f * glm::min( spacing.x, glm::min( spacing.y, spacing.z ) ) };
        for ( u32 z{}; z < dimensions.z; ++z ) {
            for ( u32 y{}; y < dimensions.y; ++y ) {
                for ( u32 x{}; x < dimensions.x; ++x ) {
                    const uint3 p{ x, y, z };
                    const auto i{ index( p ) };
                    if ( density[i] <= 0 ) {
                        continue;
                    }
                    const float3 uv{ ( float3{ p } + .5f ) / float3{ dimensions } };
                    const float3 edge{ glm::min( uv, 1.0f - uv ) * aspect };
                    const f32 inside{ glm::min( glm::max( std::sqrt( distance[i] ) - halfVoxel, 0.0f ),
                        glm::min( edge.x, glm::min( edge.y, edge.z ) ) ) };
                    voxels[i] = { glm::clamp( inside / profileDepth, 0.0f, 1.0f ),
                        glm::clamp( detailType + ( uv.y - .5f ) * .2f, 0.0f, 1.0f ),
                        glm::clamp( density[i], 0.0f, 1.0f ) };
                }
            }
        }
        return voxels;
    }

    auto CloudDensityBaker::Bake( const CloudDensityBakeDescription &description ) -> CloudDensityBake {
        using namespace core;


        const i32 size{ as<i32>( description.mResolution ) };
        const usize count{ usize( size ) * size * size };
        const auto index{ [size]( i32 x, i32 y, i32 z ) -> usize {
            return usize( x ) + usize( size ) * ( usize( y ) + usize( size ) * z );
        } };
        const auto interiorIndex{ [&]( i32 x, i32 y, i32 z ) {
            return index( glm::clamp( x, 1, size - 2 ), glm::clamp( y, 1, size - 2 ), glm::clamp( z, 1, size - 2 ) );
        } };
        const auto cells{ [&]( auto &&action ) {
            for ( i32 z{ 1 }; z < size - 1; ++z ) {
                for ( i32 y{ 1 }; y < size - 1; ++y ) {
                    for ( i32 x{ 1 }; x < size - 1; ++x ) {
                        action( x, y, z, index( x, y, z ) );
                    }
                }
            }
        } };
        const auto sample{ [&]( const auto &values, float3 position ) {
            position = glm::clamp( position, float3{ 0 }, float3{ size - 1.001f } );
            const auto lo{ glm::ivec3{ glm::floor( position ) } };
            const auto f{ glm::fract( position ) };
            typename std::decay_t<decltype( values )>::value_type result{};

            for ( i32 z{}; z < 2; ++z ) {
                for ( i32 y{}; y < 2; ++y ) {
                    for ( i32 x{}; x < 2; ++x ) {
                        const auto weight{ glm::mix( 1.0f - f, f, float3{ x, y, z } ) };
                        result += values[index( lo.x + x, lo.y + y, lo.z + z )] * weight.x * weight.y * weight.z;
                    }
                }
            }
            return result;
        } };

        eastl::vector<f32> density( count, 0 ), nextDensity( count, 0 ), source( count, 0 );
        eastl::vector<f32> pressure( count, 0 ), nextPressure( count, 0 ), divergence( count, 0 ), curlMagnitude( count, 0 );
        eastl::vector<float3> velocity( count, float3{ 0 } ), nextVelocity( count, float3{ 0 } ), curl( count, float3{ 0 } );
        u32 random{ description.mSeed };
        const auto unitRandom{ [&]() -> f32 {
            random = random * 1664525u + 1013904223u;
            return as<f32>( random >> 8 ) / 16777216.0f;
        } };

        // Regional banks start spread across the domain; cumulus retains the
        // compact updraft. Both go through the same offline fluid solver.
        const bool bank{ description.mCloudForm != CloudForm::eCumulus };
        for ( u32 puff{}; puff < ( bank ? 14u : 6u ); ++puff ) {
            const float3 centre{ ( ( bank ? .16f : .32f ) + unitRandom() * ( bank ? .68f : .36f ) ) * size,
                ( .24f + unitRandom() * .14f ) * size,
                ( ( bank ? .16f : .32f ) + unitRandom() * ( bank ? .68f : .36f ) ) * size };
            const f32 radius{ ( .13f + unitRandom() * .07f ) * size };
            cells( [&]( i32 x, i32 y, i32 z, usize i ) {
                const f32 distance{ glm::length( float3{ x, y, z } - centre ) };
                source[i] = glm::max( source[i], 1 - glm::smoothstep( radius * .55f, radius, distance ) );
            } );
        }
        density = source;
        eastl::vector<f32> temperature{ source };
        eastl::vector<f32> vapor{ source };
        for ( auto &moisture: vapor ) {
            moisture *= description.mHumidity;
        }
        eastl::vector<f32> nextTemperature( count ), nextVapor( count ), reverseDensity( count );

        // Semi-Lagrangian transport and a pressure projection follow the
        // stable-fluids workflow; this is Mikoto's small authoring solver.
        // https://www.dgp.toronto.edu/public_user/stam/reality/Research/pdf/GDC03.pdf
        // 
        // Nubis uses offline fluids to author cloud shapes, rather than running
        // a complete fluid simulation in every rendered frame.
        // https://www.guerrilla-games.com/read/nubis-cubed
        constexpr f32 dt{ .08f };
        for ( u32 step{}; step < description.mSteps; ++step ) {
            cells( [&]( i32 x, i32 y, i32 z, usize i ) {
                nextVelocity[i] = sample( velocity, float3{ x, y, z } - velocity[i] * dt );
            } );
            velocity.swap( nextVelocity );
            cells( [&]( i32 x, i32 y, i32 z, usize i ) {
                const auto dx{ ( velocity[index( x + 1, y, z )] - velocity[index( x - 1, y, z )] ) * .5f };
                const auto dy{ ( velocity[index( x, y + 1, z )] - velocity[index( x, y - 1, z )] ) * .5f };
                const auto dz{ ( velocity[index( x, y, z + 1 )] - velocity[index( x, y, z - 1 )] ) * .5f };
                curl[i] = { dy.z - dz.y, dz.x - dx.z, dx.y - dy.x };
                curlMagnitude[i] = glm::length( curl[i] );
            } );
            cells( [&]( i32 x, i32 y, i32 z, usize i ) {
                const float3 gradient{ curlMagnitude[index( x + 1, y, z )] - curlMagnitude[index( x - 1, y, z )],
                    curlMagnitude[index( x, y + 1, z )] - curlMagnitude[index( x, y - 1, z )],
                    curlMagnitude[index( x, y, z + 1 )] - curlMagnitude[index( x, y, z - 1 )] };
                const auto direction{ gradient / glm::max( glm::length( gradient ), 1e-6f ) };
                velocity[i] += dt * description.mVorticity * glm::cross( direction, curl[i] );
                velocity[i].y += dt * description.mBuoyancy * ( temperature[i] + density[i] * .5f ) * ( bank ? 3.0f : 8.0f );
                velocity[i].x += dt * description.mWindShear * ( f32( y ) / size - .3f );
                velocity[i] *= .995f;
                // A one-cell CFL bound keeps bake detail consistent when the
                // artist increases buoyancy on the same coarse grid.
                velocity[i] *= glm::min( 1.0f, 1.0f / glm::max( glm::length( velocity[i] ) * dt, 1e-6f ) );
            } );
            cells( [&]( i32 x, i32 y, i32 z, usize i ) {
                divergence[i] = .5f * ( velocity[index( x + 1, y, z )].x - velocity[index( x - 1, y, z )].x +
                                          velocity[index( x, y + 1, z )].y - velocity[index( x, y - 1, z )].y +
                                          velocity[index( x, y, z + 1 )].z - velocity[index( x, y, z - 1 )].z );
            } );
            std::fill( pressure.begin(), pressure.end(), 0.0f );
            for ( u32 iteration{}; iteration < description.mPressureIterations; ++iteration ) {
                cells( [&]( i32 x, i32 y, i32 z, usize i ) {
                    nextPressure[i] = ( pressure[interiorIndex( x + 1, y, z )] + pressure[interiorIndex( x - 1, y, z )] +
                                          pressure[interiorIndex( x, y + 1, z )] + pressure[interiorIndex( x, y - 1, z )] +
                                          pressure[interiorIndex( x, y, z + 1 )] + pressure[interiorIndex( x, y, z - 1 )] - divergence[i] ) /
                                      6;
                } );
                pressure.swap( nextPressure );
            }
            cells( [&]( i32 x, i32 y, i32 z, usize i ) {
                velocity[i] -= .5f * float3{
                    pressure[interiorIndex( x + 1, y, z )] - pressure[interiorIndex( x - 1, y, z )],
                    pressure[interiorIndex( x, y + 1, z )] - pressure[interiorIndex( x, y - 1, z )],
                    pressure[interiorIndex( x, y, z + 1 )] - pressure[interiorIndex( x, y, z - 1 )]
                };
                const auto backtrace{ float3{ x, y, z } - velocity[i] * dt };
                nextDensity[i] = sample( density, backtrace );
                nextTemperature[i] = sample( temperature, backtrace );
                nextVapor[i] = sample( vapor, backtrace );
            } );
            // A forward/backward error estimate restores detail lost by first-
            // order advection. Clamp to the departure stencil to avoid ringing.
            cells( [&]( i32 x, i32 y, i32 z, usize i ) {
                reverseDensity[i] = sample( nextDensity, float3{ x, y, z } + velocity[i] * dt );
            } );
            cells( [&]( i32 x, i32 y, i32 z, usize i ) {
                const auto departure{ glm::ivec3{ glm::floor( glm::clamp( float3{ x, y, z } - velocity[i] * dt,
                    float3{}, float3{ size - 1.001f } ) ) } };
                f32 lower{ 1 };
                f32 upper{};
                for ( i32 dz{}; dz < 2; ++dz ) {
                    for ( i32 dy{}; dy < 2; ++dy ) {
                        for ( i32 dx{}; dx < 2; ++dx ) {
                            const auto v{ density[index( departure.x + dx, departure.y + dy, departure.z + dz )] };
                            lower = glm::min( lower, v );
                            upper = glm::max( upper, v );
                        }
                    }
                }
                f32 liquid{ glm::clamp( nextDensity[i] + .5f * ( density[i] - reverseDensity[i] ), lower, upper ) };
                f32 heat{ nextTemperature[i] - dt * description.mCooling * glm::max( velocity[i].y, 0.0f ) / size };
                const f32 saturation{ glm::clamp( .55f + heat * .35f - f32( y ) / size * .25f, .1f, 1.0f ) };
                const f32 condensed{ glm::min( glm::max( nextVapor[i] - saturation, 0.0f ),
                    glm::max( nextVapor[i] - saturation, 0.0f ) * description.mCondensation * dt ) };
                const f32 evaporated{ glm::min( liquid, glm::max( saturation - nextVapor[i], 0.0f ) * description.mEvaporation * dt ) };
                const f32 injection{ step < description.mSteps / 3 ? source[i] * .04f : 0 };
                nextDensity[i] = glm::clamp( liquid * .997f + condensed - evaporated + injection, 0.0f, 1.0f );
                nextVapor[i] = glm::clamp( nextVapor[i] - condensed + evaporated, 0.0f, 1.0f );
                nextTemperature[i] = glm::clamp( heat + ( condensed - evaporated ) * .4f, 0.0f, 2.0f );
            } );
            density.swap( nextDensity );
            temperature.swap( nextTemperature );
            vapor.swap( nextVapor );
        }

        if ( bank ) {
            const auto unshaped{ density };
            const bool cirrus{ description.mCloudForm == CloudForm::eCirrus };
            const f32 thickness{ cirrus ? .18f : description.mCloudForm == CloudForm::eStratus ? .28f
                                                                                               : .5f };
            const float3 centre{ as<f32>( size ) * .5f, as<f32>( size ) * .4f, as<f32>( size ) * .5f };

            // Retain the simulated curls while authoring a flatter or streaked
            // density snapshot. Runtime consumes the resulting authored volume fields.
            cells( [&]( i32 x, i32 y, i32 z, usize i ) {
                float3 p{ float3{ x, y, z } - centre };
                p.y /= thickness;
                if ( cirrus ) {
                    p.x = p.x * .45f - p.y * .3f;
                    p.z *= 1.6f;
                }
                density[i] = sample( unshaped, p + centre );
            } );
        }

        // Leave a truly empty border for interpolation and distance-field
        // support before deriving the authored volume channels.
        cells( [&]( i32 x, i32 y, i32 z, usize i ) {
            const f32 edge{ as<f32>( glm::min( glm::min( x, size - 1 - x ),
                glm::min( glm::min( y, size - 1 - y ), glm::min( z, size - 1 - z ) ) ) ) };
            density[i] = glm::clamp( density[i] * glm::smoothstep( 1.0f, 3.0f, edge ), 0.0f, 1.0f );
        } );
        auto voxels{ BuildVolume( uint3{ description.mResolution }, density, description.mDetailType, description.mProfileDepth ) };
        return { uint3{ description.mResolution }, eastl::move( density ), description.mCloudForm, eastl::move( voxels ) };
    }

}// namespace mikoto::renderer
