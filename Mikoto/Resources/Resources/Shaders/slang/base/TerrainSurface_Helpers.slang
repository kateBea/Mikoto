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

// CPU-mirrored terrain layers and paint-weight access. The sampled layer weights drive terrain
// material blending and grass placement masks.

#pragma once

#include "Memory_Helpers.slang"

// Matches the four authored material/paint channels in TerrainSurfaceData on the CPU.
static const uint kTerrainMaterialLayerCount = 4;

struct TerrainLayerData {
    float4 mTintRoughness;
    float4 mUVNormalSlope;
    float4 mHeightRange;
    uint4 mTextures;
};

struct TerrainSurfaceData {
    TerrainLayerData mLayers[kTerrainMaterialLayerCount];
    float4 mOptions;
    uint4 mWeights;
    uint4 mEnvironment;
    float4 mLighting;
    uint4 mControls;
    uint64_t mWeightBuffer;
    uint64_t mLightBuffer;
    uint64_t mShadowBuffer;
    uint64_t mReserved;
};

// Paint channels and procedural height/slope controls meet at this shared weight query. Grass
// masks and terrain shading therefore agree on the authored layer coverage.
float4 TerrainWeights( TerrainSurfaceData surface, float2 uv, float height, float normalY ) {
    float4 weights = 0;
    if ( surface.mWeights.x >= 2 && surface.mWeights.y >= 2 ) {
        float2 pixel = saturate( uv ) * float2( surface.mWeights.xy - 1 );
        uint2 lo = uint2( floor( pixel ) );
        uint2 hi = min( lo + 1, surface.mWeights.xy - 1 );
        float2 blend = frac( pixel );
        uint offset = surface.mWeights.z;
        float4 a = LoadBufferValue<float4>( surface.mWeightBuffer, 16 * ( offset + lo.y * surface.mWeights.x + lo.x ) );
        float4 b = LoadBufferValue<float4>( surface.mWeightBuffer, 16 * ( offset + lo.y * surface.mWeights.x + hi.x ) );
        float4 c = LoadBufferValue<float4>( surface.mWeightBuffer, 16 * ( offset + hi.y * surface.mWeights.x + lo.x ) );
        float4 d = LoadBufferValue<float4>( surface.mWeightBuffer, 16 * ( offset + hi.y * surface.mWeights.x + hi.x ) );
        weights = lerp( lerp( a, b, blend.x ), lerp( c, d, blend.x ), blend.y );
    }
    else {
        float width = max( surface.mOptions.x, .001 );
        float slope = smoothstep( .15, .65, 1 - abs( normalY ) );
        for ( uint layer = 0; layer < kTerrainMaterialLayerCount; ++layer ) {
            TerrainLayerData source = surface.mLayers[layer];
            float low = smoothstep( source.mHeightRange.x - width, source.mHeightRange.x + width, height );
            float high = 1 - smoothstep( source.mHeightRange.y - width, source.mHeightRange.y + width, height );
            weights[layer] = lerp( low * high * ( 1 - slope ), slope, saturate( source.mUVNormalSlope.w ) );
        }
    }
    float total = dot( weights, 1 );
    return total > 1e-6 ? weights / total : float4( 1, 0, 0, 0 );
}
