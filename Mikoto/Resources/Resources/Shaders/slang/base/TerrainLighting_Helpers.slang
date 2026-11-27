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

// Evaluate directional shadow visibility for terrain and grass receivers. Shadow-map visibility
// is combined with the matching screen-space grass-contact layer.

#pragma once

#include "Lighting_Helpers.slang"
#include "ShadowMap_Helpers.slang"
#include "TerrainSurface_Helpers.slang"

MKT_DECLARE_TEXTURE_SRV( Texture2D<float4> )
terrainTextures;
MKT_DECLARE_TEXTURE_SRV( TextureCube<float4> )
terrainEnvironments;
MKT_DECLARE_SAMPLERS terrainSamplers;

float TerrainSunShadow( TerrainSurfaceData surface, LightInfo light, float3 world, float3 normal, float viewDepth, float3 screenPosition, bool grassReceiver = false ) {
    // Derive the receiver plane before per-pixel cascade exits. Rounded grass
    // normals and terrain normal maps affect lighting, not the triangle plane.
    float3 receiverNormal = normalize( cross( ddx( world ), ddy( world ) ) );
    receiverNormal *= dot( receiverNormal, normal ) < 0 ? -1 : 1;

    if ( light.mShadowIndex < 0 || light.mShadowIndex >= int( surface.mLighting.z ) || light.mActiveLightType != LIGHT_TYPE_DIRECTIONAL ) {
        return 1;
    }
    uint offset = uint( light.mShadowIndex ) * sizeof( ShadowRecord );
    ShadowMetadata record = LoadBufferValue<ShadowMetadata>( surface.mShadowBuffer, offset + ShadowMetadataOffset );
    if ( record.mFaceCount == 0 || viewDepth <= 0 || viewDepth > record.mCascadeSplits.w ) {
        return 1;
    }
    float contact = record.TextureID( kDirectionalReceiverSlot ) == 0xffffffffu ? 1 :
        GrassContactVisibility( terrainTextures[NonUniformResourceIndex( record.TextureID( kDirectionalReceiverSlot ) )], screenPosition, grassReceiver );
    uint cascade = 0;
    while ( cascade + 1 < record.mFaceCount && viewDepth > record.mCascadeSplits[cascade] ) {
        ++cascade;
    }
    float4x4 matrix = LoadBufferValue<float4x4>( surface.mShadowBuffer, offset + cascade * sizeof( float4x4 ) );
    float3 l = normalize( -light.mDirection.xyz );
    // Bias is authored in world metres; convert it to this cascade's depth
    // range without moving the receiver away from its geometric plane.
    float bias = ( record.mBias.x + record.mBias.y * ( 1 - saturate( dot( receiverNormal, l ) ) ) ) *
        length( matrix[2].xyz );
    float4 projected = mul( matrix, float4( world, 1 ) );
    float3 ndc = projected.xyz / projected.w;
    float2 uv = float2( ndc.x * .5 + .5, .5 - ndc.y * .5 );
    if ( any( uv < 0 ) || any( uv > 1 ) || ndc.z < 0 || ndc.z > 1 ) {
        return 1;
    }
    Texture2D<float4> shadow = terrainTextures[NonUniformResourceIndex( record.TextureID( cascade ) )];
    uint width;
    uint height;
    shadow.GetDimensions( width, height );
    int2 pixel = int2( uv * float2( width, height ) );
    float visibility = 0;
    for ( int y = -kShadowPcfRadius; y <= kShadowPcfRadius; ++y ) {
        for ( int x = -kShadowPcfRadius; x <= kShadowPcfRadius; ++x ) {
            int2 tap = pixel + int2( x, y );
            if ( any( tap < 0 ) || any( tap >= int2( width, height ) ) ) {
                visibility += 1;
                continue;
            }

            // Match opaque mesh PCF: every texel samples a different light ray.
            // Reusing one centre depth across the filter makes sloped terrain
            // and narrow imported grass triangles shadow themselves in bands.
            float2 texelUV = ( float2( tap ) + .5 ) / float2( width, height );
            float receiverDepth = DirectionalShadowReceiverDepth( matrix, texelUV, ndc, receiverNormal );
            float depth = shadow.Load( int3( tap, 0 ) ).r;
            visibility += receiverDepth < 0 || receiverDepth >= 1 || receiverDepth - bias <= depth ? 1 : 0;
        }
    }
    const int filterSide = kShadowPcfRadius * 2 + 1;
    const int filterTapCount = filterSide * filterSide;
    return min( visibility / float( filterTapCount ), contact );
}
