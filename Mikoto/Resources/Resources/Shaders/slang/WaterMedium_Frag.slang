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

// Integrate the water medium along the visible camera segment. Extinction, in-scattering and
// caustic illumination modify the scene where the camera ray passes through water.

#include "base/Camera_Helpers.slang"
#include "base/Clouds_Helpers.slang"
#include "base/LightInfo.slang"
#include "base/WaterMedium_Helpers.slang"

// Fixed midpoint steps for the camera segment through water.
static const uint kWaterMediumIntegrationSteps = 24;

// Stop marching once all remaining light is effectively extinguished at this optical-depth
// budget.
static const float kWaterMediumMaxOpticalDepth = 16.0;

struct Parameters {
    Pointer mCamera;
    Pointer mSurfaces;
    Pointer mLights;
    Pointer mShadows;
    uint mSource;
    uint mDepth;
    uint mOpaqueDepth;
    uint mSampler;
    uint mSurfaceCount;
    uint mLightCount;
    uint mShadowCount;
};

MKT_DECLARE_PUSH_CONSTANT( Parameters, parameters );
MKT_DECLARE_TEXTURE_SRV( Texture2D )
textures;
MKT_DECLARE_SAMPLERS samplers;
#include "base/MediumLighting_Helpers.slang"

// This runs after water has written the interface depth, before bloom/tonemapping.
// It affects the camera-to-interface/opaque segment exactly once.
[shader( "fragment" )]
float4 main( float2 uv : TEXCOORD0 ) : SV_Target0 {
    float4 source = textures[parameters.mSource].SampleLevel( samplers[parameters.mSampler], uv, 0 );
    CameraInfo camera = LoadBufferValue<CameraInfo>( parameters.mCamera, 0 );
    BufferReference<WaterSurface> surfaces = ( BufferReference<WaterSurface> )parameters.mSurfaces;
    uint selected = 0xffffffffu;
    float nearest = 1e30;
    for ( uint i = 0; i < parameters.mSurfaceCount; ++i ) {
        WaterSurface candidate = surfaces[i];
        float depth = WaterDepth( candidate, camera.mViewPosition.xyz );
        if ( ( candidate.mGeometry.w & kWaterUnderwaterScattering ) != 0 && depth < nearest && WaterContains( candidate, camera.mViewPosition.xyz ) ) {
            selected = i;
            nearest = depth;
        }
    }
    if ( selected == 0xffffffffu ) {
        return source;
    }
    WaterSurface surface = surfaces[selected];
    uint width;
    uint height;
    textures[parameters.mDepth].GetDimensions( width, height );
    int2 pixel = clamp( int2( uv * float2( width, height ) ), int2( 0 ), int2( width, height ) - 1 );
    float rawDepth = textures[parameters.mDepth].Load( int3( pixel, 0 ) ).r;
    float3 endpoint = WaterWorldPoint( camera, uv, min( rawDepth, .9999999 ) );
    float3 delta = endpoint - camera.mViewPosition.xyz;
    float maximum = length( delta );
    float3 direction = delta / max( maximum, 1e-6 );
    maximum = min( maximum, WaterExitDistance( surface, camera.mViewPosition.xyz, direction ) );
    // At most 24 shadowed single-scattering samples; optical depth limits invisible work.
    float3 extinction = WaterExtinction( surface );
    maximum = min( maximum, kWaterMediumMaxOpticalDepth / max( min( extinction.x, min( extinction.y, extinction.z ) ), .001 ) );
    float segment = maximum / float( kWaterMediumIntegrationSteps );

    // Integrate the same homogeneous water segment at every midpoint. Its analytic
    // transmittance/in-scattering factors are reused; only incident shadowed lighting changes
    // along the camera ray.
    float3 segmentT = exp( -extinction * segment );
    float3 segmentIntegral = WaterSegmentIntegral( extinction, segment );
    float3 transmittance = 1;
    float3 radiance = 0;
    BufferReference<LightInfo> lights = ( BufferReference<LightInfo> )parameters.mLights;
    for ( uint step = 0; step < kWaterMediumIntegrationSteps; ++step ) {
        float3 position = camera.mViewPosition.xyz + direction * ( ( step + .5 ) * segment );
        float3 lighting = 0;
        for ( uint i = 0; i < parameters.mLightCount; ++i ) {
            LightInfo light = lights[i];
            if ( light.mActiveLightType != LIGHT_TYPE_DIRECTIONAL ) {
                continue;
            }
            float3 incoming = normalize( -light.mDirection.xyz );
            float3 visibility = MediumGeometryVisibility( light, position, camera, parameters.mShadows, parameters.mShadowCount ) *
                                CloudSurfaceShadow( camera.mClouds, position, incoming );
            lighting += light.mDiffuse.rgb * light.mIntensity * visibility * WaterSunTransmittance( surface, position, incoming ) *
                        VolumePhaseHG( dot( -direction, WaterLightDirection( surface, incoming ) ), surface.mScattering.w );
        }
        radiance += transmittance * surface.mScattering.rgb * lighting * segmentIntegral;
        transmittance *= segmentT;
    }
    float opaqueDepth = textures[parameters.mOpaqueDepth].Load( int3( pixel, 0 ) ).r;
    if ( opaqueDepth < .9999999 && abs( opaqueDepth - rawDepth ) < 1e-6 && WaterContains( surface, endpoint ) ) {
        float3 caustic = 0;
        for ( uint i = 0; i < parameters.mLightCount; ++i ) {
            LightInfo light = lights[i];
            if ( light.mActiveLightType != LIGHT_TYPE_DIRECTIONAL ) {
                continue;
            }
            float3 incoming = normalize( -light.mDirection.xyz );
            float3 visibility = MediumGeometryVisibility( light, endpoint, camera, parameters.mShadows, parameters.mShadowCount ) *
                                CloudSurfaceShadow( camera.mClouds, endpoint, incoming );
            caustic += light.mDiffuse.rgb * light.mIntensity * visibility * WaterSunTransmittance( surface, endpoint, incoming ) *
                       WaterCausticPattern( surface, endpoint, incoming );
        }
        source.rgb *= 1 + min( caustic, 2.0 );
    }
    return float4( min( source.rgb * transmittance + radiance, 60000.0 ), source.a );
}
