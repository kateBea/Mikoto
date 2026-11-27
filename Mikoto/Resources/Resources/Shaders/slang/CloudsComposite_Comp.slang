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

// Upsample filtered clouds using scene-depth agreement, then compose them in scene-linear HDR.
// Atmosphere and fog are evaluated at the cloud distance before the attenuated background is
// added.

#include "base/Fog_Helpers.slang"
#include "Clouds_Common.slang"

// Composite scene-linear radiance before tone mapping. Geometry-depth weights
// prevent half-resolution clouds from bleeding over foreground silhouettes.
[shader( "compute" )]
[numthreads( kImageGroupWidth, kImageGroupHeight, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    CloudsFrameData frame = LoadBufferValue<CloudsFrameData>( parameters.mFrame, 0 );
    uint2 pixel = thread.xy;
    if ( any( pixel >= uint2( frame.mScreen.xy ) ) ) {
        return;
    }
    float2 uv = ( float2( pixel ) + .5 ) / frame.mScreen.xy;
    CameraInfo camera = LoadBufferValue<CameraInfo>( parameters.mCamera, 0 );
    float4 scene = cloudImages[parameters.mInput].Load( int3( pixel, 0 ) );
    float depth = cloudImages[parameters.mDepth].Load( int3( pixel, 0 ) ).r;
    float surfaceDistance = CloudSurfaceDistance( frame, camera, uv, depth );
    float2 source = uv * frame.mScreen.zw - .5;
    int2 base = int2( floor( source ) );
    float2 fraction = frac( source );
    float3 radiance = 0;
    float3 transmission = 0;
    float distance = 0;
    float distanceWeight = 0;
    float total = 0;
    // Reconstruct the low-resolution cloud signal with bilateral weights. Both opaque-depth
    // agreement and cloud distance protect full-resolution foreground silhouettes.
    for ( int y = 0; y < kLinearFilterSide; ++y ) {
        for ( int x = 0; x < kLinearFilterSide; ++x ) {
            int2 p = clamp( base + int2( x, y ), int2( 0 ), int2( frame.mScreen.zw ) - 1 );
            float4 scattering = cloudImages[parameters.mHistory].Load( int3( p, 0 ) );
            float4 transmittance = cloudImages[parameters.mHistoryMetadata].Load( int3( p, 0 ) );
            float2 bilinear = lerp( 1 - fraction, fraction, float2( x, y ) );
            float weight = bilinear.x * bilinear.y * exp( -abs( transmittance.a - surfaceDistance ) / max( 1, surfaceDistance * .01 ) );
            // A neighbor's cloud may be behind the full-resolution foreground.
            if ( scattering.a > surfaceDistance && surfaceDistance < frame.mMarch.y ) {
                weight = 0;
            }
            radiance += scattering.rgb * weight;
            transmission += transmittance.rgb * weight;
            total += weight;
            if ( scattering.a > 0 ) {
                distance += scattering.a * weight;
                distanceWeight += weight;
            }
        }
    }
    if ( total < 1e-6 ) {
        cloudOutputs[parameters.mOutput][pixel] = scene;
        return;
    }
    radiance /= total;
    transmission /= total;
    if ( frame.mControl.w != 0 ) {
        cloudOutputs[parameters.mOutput][pixel] = float4( radiance, scene.a );
        return;
    }
    // Apply aerial perspective to the cloud layer at its representative distance. Atmospheric
    // light fills the fraction of background removed by the cloud.
    if ( frame.mFiltering.w != 0 && distanceWeight > 1e-6 ) {
        AtmosphereData atmosphere = LoadBufferValue<AtmosphereData>( frame.mAtmosphere, 0 );
        float kilometers = distance / distanceWeight * atmosphere.mRadii.z;
        float slice = sqrt( saturate( kilometers / max( atmosphere.mGround.w, .01 ) ) ) * kAtmosAerialLastSlice;
        SamplerState sourceSampler = cloudSamplers[frame.mAtmosphereTextures.w];
        float3 air = CloudAerialVolume( frame.mAtmosphereTextures.y, uv, slice, sourceSampler );
        float3 airTransmission = CloudAerialVolume( frame.mAtmosphereTextures.z, uv, slice, sourceSampler );
        radiance = radiance * airTransmission + air * ( 1 - transmission );
    }
    if ( frame.mFogFrame != 0 && distanceWeight > 1e-6 ) {
        FogFrameData fog = LoadBufferValue<FogFrameData>( frame.mFogFrame, 0 );
        float3 fogRadiance;
        float3 fogTransmission;
        ReadIntegratedFog( cloudImages[frame.mFogTextures.x], cloudImages[frame.mFogTextures.y],
            cloudSamplers[frame.mFogTextures.z], uv, distance / distanceWeight, fog, fogRadiance, fogTransmission );
        radiance = radiance * fogTransmission + fogRadiance * ( 1 - transmission );
    }
    // Compose the integrated cloud radiance over the attenuated original scene. Transmittance
    // is a multiplicative RGB quantity, not the cloud distance stored in alpha.
    cloudOutputs[parameters.mOutput][pixel] = float4( radiance + scene.rgb * transmission, scene.a );
}
