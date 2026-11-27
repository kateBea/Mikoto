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

// Populate the cloud modeling field from authored density grids or procedural formations. The
// field is the coarse representation used for acceleration and cached lighting.

#include "Clouds_Common.slang"

// Read the authored density grid with a bounded local coordinate. Trilinear reconstruction
// supplies the coarse modeling density consumed by later acceleration and lighting stages.
float3 CloudGridSample( CloudRecord cloud, float3 uv ) {
    float3 p = clamp( uv * float3( cloud.mGrid.xyz ) - .5, 0.0, float3( cloud.mGrid.xyz - 1 ) );
    uint3 lo = uint3( floor( p ) );
    uint3 hi = min( lo + 1, cloud.mGrid.xyz - 1 );
    float3 f = frac( p );
    float3 values = 0;
    for ( uint z = 0; z < kLinearFilterSide; ++z ) {
        for ( uint y = 0; y < kLinearFilterSide; ++y ) {
            for ( uint x = 0; x < kLinearFilterSide; ++x ) {
                uint3 v = uint3( x ? hi.x : lo.x, y ? hi.y : lo.y, z ? hi.z : lo.z );
                uint index = v.x + cloud.mGrid.x * ( v.y + cloud.mGrid.y * v.z );
                float3 weight = lerp( 1 - f, f, float3( x, y, z ) );
                float3 voxel = ( cloud.mFlags.w & 4u ) != 0 ? LoadBufferValue<float3>( parameters.mModels, cloud.mGrid.w + index * sizeof( float3 ) ) : float3( 1, cloud.mShape.y, LoadBufferValue<float>( parameters.mModels, cloud.mGrid.w + index * sizeof( float ) ) );
                // Legacy scalar grids derive profile from their occupied support.
                if ( ( cloud.mFlags.w & 4u ) == 0 ) {
                    voxel.x = voxel.z > 0 ? 1 : 0;
                }
                values += voxel * weight.x * weight.y * weight.z;
            }
        }
    }
    return saturate( values );
}

[shader( "compute" )]
[numthreads( kVolumeGroupSide, kVolumeGroupSide, kVolumeGroupSide )]
void main( uint3 voxel : SV_DispatchThreadID ) {
    CloudsFrameData frame = LoadBufferValue<CloudsFrameData>( parameters.mFrame, 0 );
    if ( any( voxel >= frame.mResolution.xyz ) ) {
        return;
    }
    CloudRecord cloud = LoadCloud( frame, parameters.mVolume );
    float3 p = ( float3( voxel ) + .5 ) / float3( frame.mResolution.xyz ) - .5;
    float distance = CloudBoxDistance( p );
    float densityScale = 1;
    float detailType = cloud.mShape.y;
    float profile = 1;

    if ( cloud.mFlags.y == 3 && cloud.mGrid.w != 0 && all( cloud.mGrid.xyz > 0 ) ) {
        float3 authored = CloudGridSample( cloud, p + .5 );
        profile = authored.x;
        detailType = authored.y;
        densityScale = authored.z;
    }
    else if ( cloud.mFlags.y == 2 ) {
        // Seeded cone/lobe formations provide the macro shape; the following
        // distance bake and view-pass noise supply conservative skipping/detail.
        profile = CloudBaseDensity( frame, cloud, p );
        if ( cloud.mFlags.x == 2 ) {
            distance = max( distance, length( p ) - .48 );
        }
        profile = distance < 0 ? profile : 0;
        float3 materialPosition = CloudMaterialPosition( cloud, p );
        float3 noisePosition = materialPosition * max( cloud.mDensityDetail.y, .000001 ) * .5 + float( cloud.mFlags.z % 4096u ) * .0137;
        float typeNoise = cloudVolumes[frame.mShapeResources.x].SampleLevel( cloudSamplers[frame.mResources.z], noisePosition, 0 ).g;
        detailType = saturate( cloud.mShape.y + ( typeNoise - .5 ) * .4 * cloud.mFormation.z - max( p.y, 0 ) * .2 );
        densityScale = profile > 0 ? lerp( .35, .75, detailType ) : 0;
    }
    else if ( cloud.mFlags.x == 2 ) {
        distance = length( p ) - .5;
    }
    // The following distance bake replaces the envelope seed with its signed
    // distance. Authored and procedural noisy profiles are preserved verbatim.
    cloudVolumeOutputs[parameters.mOutput][voxel] = float4( distance, detailType, densityScale, profile );
}
