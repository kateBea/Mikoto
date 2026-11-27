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
#include <limits>

#include <EASTL/shared_ptr.h>

#include <Core/Exception.hh>

#include <Renderer/Core/TerrainGeneration.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;

    TerrainSurfaceSettings::TerrainSurfaceSettings() {
        const eastl::array<core::cstr, 4> names{ "Sand", "Grass", "Rock", "Snow" };
        const eastl::array<float3, 4> colors{ float3{ .52f, .38f, .21f }, float3{ .12f, .24f, .065f },
            float3{ .26f, .24f, .22f }, float3{ .86f, .90f, .94f } };
        const eastl::array<float2, 4> ranges{ float2{ -1, .27f }, float2{ .16f, .68f }, float2{ .5f, .9f }, float2{ .8f, 2 } };
        for ( u32 index{}; index < kTerrainSurfaceLayerCount; ++index ) {
            mLayers[index].mName = names[index];
            mLayers[index].mTint = colors[index];
            mLayers[index].mHeightRange = ranges[index];
        }
        mLayers[2].mSlopeInfluence = 1;
    }

    auto CreateTerrainHeightmap( u32 width, u32 height, const eastl::vector<f32> &heights ) -> eastl::shared_ptr<const TerrainHeightmap> {
        if ( width < 2 || height < 2 || width > 8192 || height > 8192 || heights.size() != usize{ width } * height ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid sculpted terrain image dimensions." );
        }
        eastl::vector<float4> pixels{};
        pixels.reserve( heights.size() );
        for ( const auto value: heights ) {
            pixels.emplace_back( value, value, value, 1.0f );
        }
        auto image{ asset::ImageHandle::New() };
        image->mWidth = width;
        image->mHeight = height;
        image->mChannels = 4;
        image->mFormat = asset::ImageFormat::eRGBA32_FLOAT;
        image->mBufferSpan = memory::BufferSpanHandle::New( pixels.data(), pixels.size() * sizeof( float4 ) );
        return eastl::make_shared<TerrainHeightmap>( eastl::move( image ) );
    }

    auto TerrainAutomaticWeights( f32 height, f32 normalY, const TerrainSurfaceSettings &surface ) -> float4 {
        float4 weights{};
        const float width{ glm::max( surface.mBlendWidth, .001f ) };
        const float slope{ glm::smoothstep( .15f, .65f, 1 - glm::abs( normalY ) ) };
        for ( u32 layer{}; layer < kTerrainSurfaceLayerCount; ++layer ) {
            const auto &source{ surface.mLayers[layer] };
            const float low{ glm::smoothstep( source.mHeightRange.x - width, source.mHeightRange.x + width, height ) };
            const float high{ 1 - glm::smoothstep( source.mHeightRange.y - width, source.mHeightRange.y + width, height ) };
            weights[layer] = glm::mix( low * high * ( 1 - slope ), slope, glm::clamp( source.mSlopeInfluence, 0.0f, 1.0f ) );
        }
        const auto total{ glm::dot( weights, float4{ 1 } ) };
        return total > 1e-6f ? weights / total : float4{ 1, 0, 0, 0 };
    }

    auto IntersectTerrain( const WorldDescription &world, const eastl::shared_ptr<const TerrainHeightmap> &heights,
        float3 origin, float3 direction, float3 &position ) -> bool {
        if ( !IsTerrainDescriptionValid( world ) || glm::dot( direction, direction ) < 1e-12f ) {
            return false;
        }
        const auto size{ world.GetWorldSize() };
        float entry{};
        float exit{ 1 };
        for ( const int axis: { 0, 2 } ) {
            const float minimum{ axis == 0 ? world.mOrigin.x : world.mOrigin.y };
            const float maximum{ minimum + ( axis == 0 ? size.x : size.y ) };
            if ( glm::abs( direction[axis] ) < 1e-10f ) {
                if ( origin[axis] < minimum || origin[axis] > maximum ) {
                    return false;
                }
            }
            else {
                const float a{ ( minimum - origin[axis] ) / direction[axis] };
                const float b{ ( maximum - origin[axis] ) / direction[axis] };
                entry = glm::max( entry, glm::min( a, b ) );
                exit = glm::min( exit, glm::max( a, b ) );
            }
        }
        if ( entry > exit ) {
            return false;
        }
        const int columns{ as<int>( world.GetResolutionX() ) - 1 };
        const int rows{ as<int>( world.GetResolutionY() ) - 1 };
        const float3 start{ origin + direction * glm::min( entry + 1e-7f, exit ) };
        int x{ glm::clamp( as<int>( std::floor( ( start.x - world.mOrigin.x ) / world.mVertexSpacing ) ), 0, columns - 1 ) };
        int z{ glm::clamp( as<int>( std::floor( ( start.z - world.mOrigin.y ) / world.mVertexSpacing ) ), 0, rows - 1 ) };
        const int dx{ direction.x >= 0 ? 1 : -1 };
        const int dz{ direction.z >= 0 ? 1 : -1 };
        const float infinity{ std::numeric_limits<float>::infinity() };
        const float tx{ glm::abs( direction.x ) > 1e-10f ? world.mVertexSpacing / glm::abs( direction.x ) : infinity };
        const float tz{ glm::abs( direction.z ) > 1e-10f ? world.mVertexSpacing / glm::abs( direction.z ) : infinity };
        float nextX{ tx != infinity ? ( world.mOrigin.x + ( x + ( dx > 0 ? 1 : 0 ) ) * world.mVertexSpacing - origin.x ) / direction.x : infinity };
        float nextZ{ tz != infinity ? ( world.mOrigin.y + ( z + ( dz > 0 ? 1 : 0 ) ) * world.mVertexSpacing - origin.z ) / direction.z : infinity };
        const auto vertex{ [&]( int column, int row ) {
            const float height{ heights ? heights->Sample( float2{ float( column ) / columns, float( row ) / rows } ) : 0 };
            return float3{ world.mOrigin.x + column * world.mVertexSpacing,
                world.mBaseElevation + height * world.mHeightScale, world.mOrigin.y + row * world.mVertexSpacing };
        } };
        const auto triangle{ [&]( float3 a, float3 b, float3 c, float limit ) {
            const float3 e1{ b - a };
            const float3 e2{ c - a };
            const float3 p{ glm::cross( direction, e2 ) };
            const float det{ glm::dot( e1, p ) };
            if ( glm::abs( det ) < 1e-10f ) {
                return false;
            }
            const float3 offset{ origin - a };
            const float u{ glm::dot( offset, p ) / det };
            const float3 q{ glm::cross( offset, e1 ) };
            const float v{ glm::dot( direction, q ) / det };
            const float fraction{ glm::dot( e2, q ) / det };
            if ( u < -1e-5f || v < -1e-5f || u + v > 1.00001f || fraction < entry - 1e-6f || fraction > limit + 1e-6f ) {
                return false;
            }
            position = origin + direction * fraction;
            return true;
        } };
        // Grid traversal is bounded by crossed cells, rather than all mesh triangles.
        for ( int cell{}; cell < columns + rows + 4 && x >= 0 && x < columns && z >= 0 && z < rows; ++cell ) {
            const float limit{ glm::min( exit, glm::min( nextX, nextZ ) ) };
            const auto a{ vertex( x, z ) };
            const auto b{ vertex( x + 1, z ) };
            const auto c{ vertex( x, z + 1 ) };
            const auto d{ vertex( x + 1, z + 1 ) };
            if ( triangle( a, c, b, limit ) || triangle( b, c, d, limit ) ) {
                return true;
            }
            if ( limit >= exit ) {
                break;
            }
            if ( nextX < nextZ ) {
                x += dx;
                nextX += tx;
            }
            else {
                z += dz;
                nextZ += tz;
            }
        }
        return false;
    }

}// namespace mikoto::renderer
