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

// Test scene geometry visibility along a medium-lighting ray. Directional and finite-light
// queries use the matching shadow-record projection and receiver depth.

#pragma once

#include "ShadowMap_Helpers.slang"

float MediumGeometryVisibility( LightInfo light, float3 position, CameraInfo camera, uint64_t shadows, uint shadowCount ) {
    if ( light.mShadowIndex < 0 || uint( light.mShadowIndex ) >= shadowCount ) {
        return 1.0;
    }
    uint offset = uint( light.mShadowIndex ) * sizeof( ShadowRecord );
    ShadowMetadata shadow = LoadBufferValue<ShadowMetadata>( shadows, offset + ShadowMetadataOffset );
    uint face = 0;
    if ( light.mActiveLightType == LIGHT_TYPE_DIRECTIONAL ) {
        float depth = abs( mul( camera.mViewMatrix, float4( position, 1.0 ) ).z );
        if ( depth > shadow.mCascadeSplits.x ) {
            face = 1;
        }
        if ( depth > shadow.mCascadeSplits.y ) {
            face = 2;
        }
        if ( depth > shadow.mCascadeSplits.z ) {
            face = 3;
        }
        face = min( face, max( shadow.mFaceCount, 1u ) - 1 );
    }
    else if ( light.mActiveLightType == LIGHT_TYPE_POINT ) {
        float3 delta = position - shadow.mPositionRange.xyz;
        float3 axis = abs( delta );
        face = axis.x >= max( axis.y, axis.z ) ? ( delta.x >= 0.0 ? 0u : 1u ) : ( axis.y >= axis.z ? ( delta.y >= 0.0 ? 2u : 3u ) : ( delta.z >= 0.0 ? 4u : 5u ) );
    }
    if ( face >= shadow.mFaceCount || shadow.TextureID( face ) == 0xffffffffu ) {
        return 1.0;
    }
    float4x4 matrix = LoadBufferValue<float4x4>( shadows, offset + face * sizeof( float4x4 ) );
    float4 clip = mul( matrix, float4( position, 1.0 ) );
    if ( clip.w <= 0.0 ) {
        return 1.0;
    }
    float3 ndc = clip.xyz / clip.w;
    float2 uv = float2( ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5 );
    if ( any( uv < 0.0 ) || any( uv > 1.0 ) || ndc.z < 0.0 || ndc.z > 1.0 ) {
        return 1.0;
    }
    float depth = light.mActiveLightType == LIGHT_TYPE_DIRECTIONAL ? ndc.z : LocalShadowDepth( position, shadow.mPositionRange );
    float bias = shadow.mBias.x * ( light.mActiveLightType == LIGHT_TYPE_DIRECTIONAL ? length( matrix[2].xyz ) : 1.0 );
    uint textureID = shadow.TextureID( face );
    uint width;
    uint height;
    textures[textureID].GetDimensions( width, height );
    int2 pixel = int2( uv * float2( width, height ) - 0.5 );
    float visible = 0.0;
    for ( int y = 0; y < kLinearFilterSide; ++y ) {
        for ( int x = 0; x < kLinearFilterSide; ++x ) {
            int2 tap = clamp( pixel + int2( x, y ), int2( 0 ), int2( width, height ) - 1 );
            visible += depth - max( bias, 1e-6 ) <= textures[textureID].Load( int3( tap, 0 ) ).r ? 0.25 : 0.0;
        }
    }
    return visible;
}
