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

#include <EASTL/shared_ptr.h>

#include <Application/TerrainTools.hh>

namespace mikoto::editor {

    using namespace mikoto::core;

    static auto CaptureTerrain( const scene::LandscapeComponent &landscape ) -> TerrainEditSnapshot {
        return { landscape.GetSculptedHeightmap(), landscape.GetPaintMap() };
    }

    auto TerrainToolState::SetTarget( scene::Scene *scene, u64 entity ) -> void {
        if ( mScene.GetPtr() != scene || mEntity != entity ) {
            mScene = scene::SceneHandle{ scene };
            mEntity = entity;
            mStrokeActive = false;
            mBefore = {};
            mHistory.clear();
            mHistoryIndex = 0;
        }
    }

    auto TerrainToolState::BeginStroke( const scene::LandscapeComponent &landscape, f32 height ) -> void {
        mBefore = CaptureTerrain( landscape );
        mFlattenHeight = height;
        mStrokeActive = true;
    }

    auto TerrainToolState::EndStroke( const scene::LandscapeComponent &landscape ) -> void {
        if ( !mStrokeActive ) {
            return;
        }
        mStrokeActive = false;
        const auto after{ CaptureTerrain( landscape ) };
        if ( after.mHeightmap == mBefore.mHeightmap && after.mPaintMap == mBefore.mPaintMap ) {
            mBefore = {};
            return;
        }
        mHistory.resize( mHistoryIndex );
        if ( mHistory.size() == 8 ) {
            mHistory.erase( mHistory.begin() );
        }
        mHistory.push_back( { mBefore, after } );
        mHistoryIndex = mHistory.size();
        mBefore = {};
    }

    auto TerrainToolState::Undo( scene::LandscapeComponent &landscape ) -> void {
        EndStroke( landscape );
        if ( mHistoryIndex ) {
            const auto &snapshot{ mHistory[--mHistoryIndex].mBefore };
            landscape.SetSculptedHeightmap( snapshot.mHeightmap );
            landscape.SetPaintMap( snapshot.mPaintMap );
        }
    }

    auto TerrainToolState::Redo( scene::LandscapeComponent &landscape ) -> void {
        if ( !mStrokeActive && mHistoryIndex < mHistory.size() ) {
            const auto &snapshot{ mHistory[mHistoryIndex++].mAfter };
            landscape.SetSculptedHeightmap( snapshot.mHeightmap );
            landscape.SetPaintMap( snapshot.mPaintMap );
        }
    }

    auto ApplyTerrainBrush( scene::LandscapeComponent &landscape, const float4x4 &transform,
        const TerrainBrushSettings &brush, float3 center, f32 elapsed, bool reverse, f32 flattenHeight ) -> bool {
        const auto world{ landscape.GetWorldDescription() };
        const auto source{ landscape.GetHeightmap() };
        const auto paint{ landscape.GetPaintMap() };
        const bool sculpt{ brush.mMode == TerrainEditMode::eSculpt };
        if ( brush.mMode == TerrainEditMode::eGenerate || elapsed <= 0 || brush.mRadius <= 0 ||
             ( sculpt && world.mHeightScale <= .0001f ) || ( source && ( source->mWidth > 2049 || source->mHeight > 2049 ) ) ) {
            return false;
        }
        if ( const auto noise{ landscape.GetNoiseGeneration() }; noise && !source ) {
            return false;
        }
        const u32 width{ sculpt ? source ? source->mWidth : glm::min( world.GetResolutionX(), 1025u ) : paint ? paint->mWidth
                                                                                                              : glm::min( world.GetResolutionX(), 1025u ) };
        const u32 height{ sculpt ? source ? source->mHeight : glm::min( world.GetResolutionY(), 1025u ) : paint ? paint->mHeight
                                                                                                                : glm::min( world.GetResolutionY(), 1025u ) };
        const auto size{ world.GetWorldSize() };
        const auto inverse{ glm::inverse( transform ) };
        const float3 localCenter{ inverse * float4{ center, 1 } };
        const float localRadius{ brush.mRadius * ( glm::length( float3{ inverse[0] } ) + glm::length( float3{ inverse[2] } ) ) };
        const auto centerUV{ ( float2{ localCenter.x, localCenter.z } - world.mOrigin ) / size };
        const auto radiusUV{ float2{ localRadius } / size };
        const int x0{ glm::clamp( as<int>( std::floor( ( centerUV.x - radiusUV.x ) * ( width - 1 ) ) ), 0, as<int>( width - 1 ) ) };
        const int x1{ glm::clamp( as<int>( std::ceil( ( centerUV.x + radiusUV.x ) * ( width - 1 ) ) ), 0, as<int>( width - 1 ) ) };
        const int y0{ glm::clamp( as<int>( std::floor( ( centerUV.y - radiusUV.y ) * ( height - 1 ) ) ), 0, as<int>( height - 1 ) ) };
        const int y1{ glm::clamp( as<int>( std::ceil( ( centerUV.y + radiusUV.y ) * ( height - 1 ) ) ), 0, as<int>( height - 1 ) ) };
        const auto sample{ [&]( float2 uv ) {
            return source ? source->Sample( uv ) : 0.0f;
        } };
        eastl::vector<f32> heights{};
        auto weights{ eastl::make_shared<renderer::TerrainPaintMap>() };
        if ( sculpt ) {
            heights.reserve( usize{ width } * height );
            for ( u32 y{}; y < height; ++y ) {
                for ( u32 x{}; x < width; ++x ) {
                    heights.push_back( sample( { float( x ) / ( width - 1 ), float( y ) / ( height - 1 ) } ) );
                }
            }
        }
        else {
            weights->mWidth = width;
            weights->mHeight = height;
            if ( paint ) {
                weights->mWeights = paint->mWeights;
            }
            else {
                weights->mWeights.reserve( usize{ width } * height );
                const float2 texel{ 1.0f / ( width - 1 ), 1.0f / ( height - 1 ) };
                for ( u32 y{}; y < height; ++y ) {
                    for ( u32 x{}; x < width; ++x ) {
                        const float2 uv{ float( x ) / ( width - 1 ), float( y ) / ( height - 1 ) };
                        const float dx{ ( sample( uv + float2{ texel.x, 0 } ) - sample( uv - float2{ texel.x, 0 } ) ) * world.mHeightScale / ( 2 * size.x * texel.x ) };
                        const float dz{ ( sample( uv + float2{ 0, texel.y } ) - sample( uv - float2{ 0, texel.y } ) ) * world.mHeightScale / ( 2 * size.y * texel.y ) };
                        weights->mWeights.push_back( renderer::TerrainAutomaticWeights( sample( uv ), 1 / std::sqrt( 1 + dx * dx + dz * dz ), landscape.GetSurfaceSettings() ) );
                    }
                }
            }
        }
        bool changed{};
        const float strength{ glm::clamp( brush.mStrength, 0.0f, 1.0f ) * glm::min( elapsed, .1f ) };
        const float scaleY{ glm::max( glm::length( float3{ transform[1] } ), .001f ) };
        for ( int y{ y0 }; y <= y1; ++y ) {
            for ( int x{ x0 }; x <= x1; ++x ) {
                const float2 uv{ float( x ) / ( width - 1 ), float( y ) / ( height - 1 ) };
                const float normalized{ sample( uv ) };
                const float3 point{ transform * float4{ world.mOrigin.x + uv.x * size.x, world.mBaseElevation + normalized * world.mHeightScale, world.mOrigin.y + uv.y * size.y, 1 } };
                const float distance{ glm::length( float2{ point.x - center.x, point.z - center.z } ) / brush.mRadius };
                if ( distance >= 1 ) {
                    continue;
                }
                const float inner{ 1 - glm::clamp( brush.mFalloff, .001f, 1.0f ) };
                const float influence{ 1 - glm::smoothstep( inner, 1.0f, distance ) };
                const auto index{ usize{ as<u32>( y ) } * width + x };
                if ( sculpt ) {
                    float value{ normalized };
                    if ( brush.mSculptTool == TerrainSculptTool::eRaiseLower ) {
                        value += ( reverse ? -1 : 1 ) * strength * influence * brush.mRadius * .2f / ( world.mHeightScale * scaleY );
                    }
                    else {
                        const float2 texel{ 1.0f / ( width - 1 ), 1.0f / ( height - 1 ) };
                        const float target{ brush.mSculptTool == TerrainSculptTool::eFlatten ? flattenHeight : ( sample( uv + float2{ texel.x, 0 } ) + sample( uv - float2{ texel.x, 0 } ) + sample( uv + float2{ 0, texel.y } ) + sample( uv - float2{ 0, texel.y } ) ) * .25f };
                        value = glm::mix( normalized, target, glm::clamp( strength * influence * 8, 0.0f, 1.0f ) );
                    }
                    heights[index] = value;
                    changed |= value != normalized;
                }
                else {
                    auto &weight{ weights->mWeights[index] };
                    const auto layer{ glm::min( brush.mLayer, renderer::kTerrainSurfaceLayerCount - 1 ) };
                    const float blend{ glm::clamp( strength * influence * 4, 0.0f, 1.0f ) };
                    float4 target{};
                    if ( reverse ) {
                        target = weight;
                        target[layer] = 0;
                        const float total{ glm::dot( target, float4{ 1 } ) };
                        if ( total > 1e-6f ) {
                            target /= total;
                        }
                        else {
                            target[( layer + 1 ) % renderer::kTerrainSurfaceLayerCount] = 1;
                        }
                    }
                    else {
                        target[layer] = 1;
                    }
                    const auto before{ weight };
                    weight = glm::mix( weight, target, blend );
                    changed |= weight != before;
                }
            }
        }
        if ( changed ) {
            if ( sculpt ) {
                landscape.SetSculptedHeightmap( renderer::CreateTerrainHeightmap( width, height, heights ) );
            }
            else {
                landscape.SetPaintMap( weights );
            }
        }
        return changed;
    }

}// namespace mikoto::editor
