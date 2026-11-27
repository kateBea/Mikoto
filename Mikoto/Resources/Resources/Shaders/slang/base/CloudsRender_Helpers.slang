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

// Combine cached and detailed cloud illumination for the render march. Step-size selection
// respects the cloud voxel scale while light evaluation accounts for sun and local sources.

#pragma once
#include "CloudLighting_Helpers.slang"

// Cached sun/sky integrals and an SDF-guided view march follow the shared
// structure of Cumulus and Frostnova. Transport remains scene-linear and RGB.
// https://github.com/rubenaryo/Cumulus
// https://github.com/YueZhang1027/CIS5650-Final-Project-Frostnova
// Convert sun radiance into incident cloud illumination, including atmosphere and geometry
// visibility along the sunlight direction.
// Three approximate higher scattering orders decay geometrically; two detailed prefix samples
// replace the cached sun prefix.
static const uint kCloudMultipleScatteringOrders = 3;
static const uint kCloudSunDetailSamples = 2;
static const float kCloudSunDetailWeight = .5;
static const float kCloudMultipleScatteringDecay = .5;

float3 CloudIncidentSun( CloudsFrameData frame, CloudRecord cloud, float3 position,
    float cosine, float4 field, float3 opticalDepth ) {
    float3 sun = frame.mSunColor.rgb;
    if ( frame.mFiltering.z != 0 ) {
        AtmosphereData atmosphere = LoadBufferValue<AtmosphereData>( frame.mAtmosphere, 0 );
        sun = AtmosSurfaceSun( atmosphere, position, cloudWeatherMaps[frame.mAtmosphereTextures.x], cloudSamplers[frame.mAtmosphereTextures.w] );
    }
    float g = cloud.mScatteringPhase.w;
    float phase = .85 * CloudPhase( cosine, g ) + .15 * CloudPhase( cosine, -.2 );
    float3 energy = exp( -opticalDepth ) * phase;
    float weight = saturate( frame.mMarch.w );
    float attenuation = .5;
    float profile = cloud.mFlags.y >= 2 ? saturate( field.a ) : 1;
    float probability = lerp( 1, profile, saturate( frame.mDetail.x ) );

    // Approximate successive scattering orders with weaker extinction and a less directional
    // phase lobe. These low-order terms fill dense cloud interiors without tracing secondary
    // camera paths.
    for ( uint order = 1; order <= kCloudMultipleScatteringOrders; ++order ) {
        g *= .5;
        float towardSun = saturate( cosine );
        float interior = saturate( -field.r / max( cloud.mShape.z * .08, .001 ) );
        float relaxation = lerp( attenuation, attenuation * lerp( 1, .2, interior ), towardSun * saturate( frame.mDetail.y ) );
        energy += probability * weight * exp( -opticalDepth * relaxation ) * CloudPhase( cosine, g );
        weight *= kCloudMultipleScatteringDecay;
        attenuation *= kCloudMultipleScatteringDecay;
    }
    return sun * energy;
}

// Integrate a short detailed sun path at the current cloud sample. This refines the low-
// frequency cache near density structure.
float3 CloudDetailedSunDepth( CloudsFrameData frame, CloudRecord cloud, float3 position, float3 cached ) {
    float2 interval;
    if ( !CloudInterval( cloud, position, frame.mSunDirection.xyz, interval ) ) {
        return cached;
    }
    float3 extent = CloudMaterialPosition( cloud, float3( 1 ) );
    float3 voxel = extent / float3( frame.mResolution.xyz );
    float step = max( min( voxel.x, min( voxel.y, voxel.z ) ), .001 );
    // Keep the lookup within the cache's voxel-centre domain. Near a boundary
    // the unshifted cache already contains the short remaining ray.
    float length = min( step * 2, max( interval.y - step, 0 ) );
    if ( length <= .001 ) {
        return cached;
    }
    float3 depth = 0;
    for ( uint i = 0; i < kCloudSunDetailSamples; ++i ) {
        float3 sample = position + frame.mSunDirection.xyz * ( ( i + .5 ) * length * kCloudSunDetailWeight );
        depth += CloudExtinction( frame, sample, true ) * length * kCloudSunDetailWeight;
    }
    float3 tail = position + frame.mSunDirection.xyz * length;
    float3 uv = mul( cloud.mWorldToLocal, float4( tail, 1 ) ).xyz + .5;
    // The detailed prefix replaces the cached prefix; never add it at the same
    // point, which would double-count extinction and darken cloud edges.
    return depth + max( cloudVolumes[NonUniformResourceIndex( cloud.mTextures.y )].SampleLevel(
                                                                                     cloudSamplers[frame.mResources.y], uv, 0 )
                            .rgb,
                       0.0 );
}

// Combine directional/local incident light with the cloud phase response at this camera-ray
// sample. The camera integrator multiplies it by scattering density and segment transmission.
float3 CloudReceiverLighting( CloudsFrameData frame, CloudRecord cloud, uint index,
    float3 position, float3 view, float4 field ) {
    float3 uv = saturate( mul( cloud.mWorldToLocal, float4( position, 1 ) ).xyz + .5 );
    float4 cache = cloudVolumes[NonUniformResourceIndex( cloud.mTextures.y )].SampleLevel( cloudSamplers[frame.mResources.y], uv, 0 );
    bool receive = ( cloud.mFlags.w & 2u ) != 0;
    float3 opticalDepth = receive ? CloudDetailedSunDepth( frame, cloud, position, max( cache.rgb, 0.0 ) ) : float3( 0 );
    float visible = receive ? CloudMeshVisibility( frame, index, position ) : 1;
    float3 direct = CloudIncidentSun( frame, cloud, position, dot( view, frame.mSunDirection.xyz ), field, opticalDepth ) * visible;
    float profile = cloud.mFlags.y >= 2 ? saturate( field.a ) : 0;
    float height = uv.y;
    float3 sky = frame.mAmbientColor.rgb;
    float3 ground = sky * float3( .24, .22, .20 );
    float3 ambient = lerp( ground, sky, sqrt( saturate( height ) ) ) * frame.mAmbientColor.w;
    ambient *= lerp( 1, sqrt( saturate( 1 - profile ) ), saturate( frame.mDetail.x ) ) * exp( -max( cache.a, 0.0 ) );
    float3 local = frame.mLightingControl.x > 0 ? cloudVolumes[NonUniformResourceIndex( frame.mLocalTextures[index / 4][index % 4] )].SampleLevel(
                                                                                                                                       cloudSamplers[frame.mResources.y], uv, 0 )
                                                     .rgb
                                               : float3( 0 );
    return direct + ambient + local;
}

// Choose a conservative world-space lower step scale across active clouds. Small/thin
// formations should not disappear when larger volumes are present.
float CloudMinimumVoxelStep( CloudsFrameData frame ) {
    float step = max( frame.mMarch.x, .001 );
    for ( uint index = 0; index < frame.mControl.x; ++index ) {
        CloudRecord cloud = LoadCloud( frame, index );
        float3 extent = CloudMaterialPosition( cloud, float3( 1 ) );
        float3 voxel = extent / frame.mResolution.xyz;
        step = min( step, max( min( voxel.x, min( voxel.y, voxel.z ) ) * .5, .001 ) );
    }
    return step;
}
