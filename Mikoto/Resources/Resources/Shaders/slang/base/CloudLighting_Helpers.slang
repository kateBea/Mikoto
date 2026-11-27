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

// Cloud geometry visibility and local-light optical-depth integration. These queries attenuate
// light along its path through scene geometry and the cloud medium.

#pragma once

#include "Clouds_Helpers.slang"
#include "ShadowMap_Helpers.slang"

struct CloudLocalLight {
    float4 mPositionRange;
    float4 mDirectionOuter;
    float4 mColorIntensity;
    float4 mParameters;
    uint4 mControl;
};

// Project a cloud lighting point into the scene-geometry shadow map. This handles geometry
// blocking incident sunlight before cloud self-extinction is applied.
float CloudMeshVisibility( CloudsFrameData frame, uint record, float3 position ) {
    if ( frame.mMeshShadows == 0 ) {
        return 1;
    }
    uint offset = record * sizeof( ShadowRecord );
    ShadowMetadata shadow = LoadBufferValue<ShadowMetadata>( frame.mMeshShadows, offset + ShadowMetadataOffset );
    if ( shadow.mFaceCount == 0 ) {
        return 1;
    }
    uint face = 0;
    if ( shadow.mLightType == 1 ) {
        float3 delta = position - shadow.mPositionRange.xyz;
        float3 a = abs( delta );
        face = a.x >= a.y && a.x >= a.z ? ( delta.x >= 0 ? 0 : 1 ) : a.y >= a.z ? ( delta.y >= 0 ? 2 : 3 )
                                                                                : ( delta.z >= 0 ? 4 : 5 );
    }
    float4x4 matrix = LoadBufferValue<float4x4>( frame.mMeshShadows, offset + face * sizeof( float4x4 ) );
    float4 clip = mul( matrix, float4( position, 1 ) );
    if ( clip.w <= 0 ) {
        return 1;
    }
    float3 ndc = clip.xyz / clip.w;
    if ( any( abs( ndc.xy ) > 1 ) || ndc.z < 0 || ndc.z > 1 ) {
        return 1;
    }
    float receiver = shadow.mLightType == 0 ? ndc.z : length( position - shadow.mPositionRange.xyz ) / shadow.mPositionRange.w;
    uint texture = shadow.TextureID( face );
    uint width;
    uint height;
    cloudWeatherMaps[NonUniformResourceIndex( texture )].GetDimensions( width, height );
    int2 pixel = int2( float2( ndc.x * .5 + .5, .5 - ndc.y * .5 ) * float2( width, height ) );
    float visibility = 0;
    for ( int y = -1; y <= 1; ++y ) {
        for ( int x = -1; x <= 1; ++x ) {
            int2 tap = clamp( pixel + int2( x, y ), int2( 0 ), int2( width, height ) - 1 );
            float depth = cloudWeatherMaps[NonUniformResourceIndex( texture )].Load( int3( tap, 0 ) ).r;
            visibility += receiver - shadow.mBias.x <= depth ? 1.0 : 0.0;
        }
    }
    return visibility / 9;
}

// Integrate only occupied cloud intervals. Empty kilometres between a lamp and
// a small formation cannot consume the entire light-ray sample budget.
// Accumulate extinction from a cloud point toward a local emitter. The finite light distance
// limits the integration interval.
float3 CloudLocalOpticalDepth( CloudsFrameData frame, float3 origin, float3 direction, float maximumDistance ) {
    float3 result = 0;
    for ( uint index = 0; index < frame.mControl.x; ++index ) {
        CloudRecord cloud = LoadCloud( frame, index );
        if ( ( cloud.mFlags.w & 1u ) == 0 ) {
            continue;
        }
        float2 interval;
        if ( !CloudInterval( cloud, origin, direction, interval ) ) {
            continue;
        }
        float start = max( interval.x, 0 );
        float end = min( interval.y, maximumDistance );
        if ( end <= start ) {
            continue;
        }
        uint count = clamp( frame.mResolution.w, 4u, 32u );
        float stepLength = ( end - start ) / count;
        for ( uint i = 0; i < count; ++i ) {
            float3 p = origin + direction * ( start + ( i + .5 ) * stepLength );
            result += CloudDensity( frame, cloud, p, CloudField( frame, cloud, p ) ) * cloud.mExtinction.rgb * stepLength;
        }
        if ( all( result > 12 ) ) {
            break;
        }
    }
    return min( result, 80.0 );
}
