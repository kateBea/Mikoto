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

// Grass draw parameters and conservative Hi-Z occlusion. Placement, color and contact-depth
// passes share the same blade/mesh record addressing.

#pragma once

#include "Camera_Helpers.slang"
#include "Memory_Helpers.slang"
#include "Grass_Shared.slang"
#include "TerrainSurface_Helpers.slang"
#include "Wind_Helpers.slang"

struct GrassParameters {
    uint64_t mDescriptions;
    uint64_t mVertices;
    uint64_t mCamera;
    uint64_t mBlades;
    uint64_t mMeshes;
    uint64_t mHiZ;
    uint mArguments;
    uint mTerrain;
    uint mLod;
    uint mFinalize;
    uint mHiZWidth;
    uint mHiZHeight;
    uint mHiZLevels;
    uint mDispatchWidth;
};

MKT_DECLARE_PUSH_CONSTANT( GrassParameters, parameters );

// Conservative current-frame Hi-Z test. The farthest depth of every touched cell
// must be in front of the entire wind-expanded blade bound before rejecting it.
// The pyramid contains opaque geometry/terrain only, never grass from a prior frame.
bool GrassOccluded( CameraInfo camera, float3 root, float radius ) {
    if ( parameters.mHiZ == 0 || parameters.mHiZLevels == 0 ) {
        return false;
    }
    float2 minimum = float2( 1, 1 );
    float2 maximum = float2( -1, -1 );
    float nearest = 1;
    for ( uint corner = 0; corner < kBoxCornerCount; ++corner ) {
        float3 offset = float3( ( corner & 1 ) ? 1 : -1, ( corner & 2 ) ? 1 : -1, ( corner & 4 ) ? 1 : -1 ) * radius;
        float4 clip = CameraClipPosition( float4( root + offset, 1 ), camera );
        if ( clip.w <= 0 || clip.z <= 0 ) {
            return false;
        }
        float3 ndc = clip.xyz / clip.w;
        minimum = min( minimum, ndc.xy );
        maximum = max( maximum, ndc.xy );
        nearest = min( nearest, ndc.z );
    }
    // Mikoto keeps Y-up projection matrices. Both the negative-height Vulkan
    // viewport and the D3D viewport map positive NDC Y to the top image row.
    float2 uvMinimum = float2( minimum.x * .5 + .5, .5 - maximum.y * .5 );
    float2 uvMaximum = float2( maximum.x * .5 + .5, .5 - minimum.y * .5 );
    uint2 sourceSize = uint2( parameters.mHiZWidth, parameters.mHiZHeight );
    uint2 size = ( sourceSize + 1 ) / 2;
    float2 footprintSize = ( uvMaximum - uvMinimum ) * float2( sourceSize ) * .5;
    float footprint = max( footprintSize.x, footprintSize.y );
    uint level = min( uint( ceil( log2( max( footprint, 1 ) ) ) ), parameters.mHiZLevels - 1 );
    uint first = 0;
    for ( uint i = 0; i < level; ++i ) {
        first += size.x * size.y;
        size = max( ( size + 1 ) / 2, 1 );
    }
    // Address physical 2^(level+1)-pixel footprints, not normalized coarse
    // dimensions: ceil-reduced odd sizes have a shorter final cell.
    float divisor = exp2( float( level + 1 ) );
    uint2 lo = min( uint2( saturate( uvMinimum ) * float2( sourceSize ) / divisor ), size - 1 );
    uint2 hi = min( uint2( saturate( uvMaximum ) * float2( sourceSize ) / divisor ), size - 1 );
    // Odd-sized levels can cover a third cell after ceil division. Fall back
    // rather than claiming occlusion from an incomplete set of depth samples.
    if ( any( hi - lo > 1 ) ) {
        return false;
    }
    float farthest = 0;
    for ( uint y = lo.y; y <= hi.y; ++y ) {
        for ( uint x = lo.x; x <= hi.x; ++x ) {
            farthest = max( farthest, LoadBufferValue<float>( parameters.mHiZ, ( first + y * size.x + x ) * 4 ) );
        }
    }
    return nearest > farthest + .001;
}
