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

// Generate atmosphere lookup data selected by the pass parameters. Direction and altitude are
// reconstructed from texel coordinates before integrating the medium.

#pragma once

#include "Atmosphere_Common.slang"

// Stage IDs match the CPU LUT draws; transmittance and sky radiance use different integration budgets.
static const uint kAtmosTransmittanceStage = 0;
static const uint kAtmosEnvironmentStage = 3;
static const uint kAtmosTransmittanceSteps = 64;
static const uint kAtmosSkyIntegrationSteps = 32;

[shader( "fragment" )]
float4 main( float2 uv : TEXCOORD0 ) : SV_Target0 {
    AtmosphereData a = Atmosphere();
    if ( parameters.mStage == kAtmosTransmittanceStage ) {
        float3 origin;
        float3 direction;
        AtmosTransmittanceRay( a, uv, origin, direction );

        // Transmittance is only defined for paths reaching the top boundary.
        // Use the far root here: an inward ray starting exactly on the top
        // boundary must cross the atmosphere rather than stop at t=0.
        float b = dot( origin, direction );
        float distance = max( 0, -b + sqrt( max( 0, b * b - dot( origin, origin ) + a.mRadii.y * a.mRadii.y ) ) );
        float3 opticalDepth = 0;
        for ( uint i = 0; i < kAtmosTransmittanceSteps; ++i ) {
            float3 position = origin + direction * ( distance * ( i + 0.5 ) / float( kAtmosTransmittanceSteps ) );
            opticalDepth += AtmosSampleMedium( a, position ).mExtinction * ( distance / float( kAtmosTransmittanceSteps ) );
        }

        return float4( exp( -opticalDepth ), 1 );
    }

    float3 origin;
    float3 direction;
    if ( parameters.mStage == kAtmosEnvironmentStage ) {
        // IBL uses a fixed observer at ground level, so flying the editor
        // camera does not continuously rerun expensive cubemap convolution.
        origin = float3( 0, a.mRadii.x + AtmosGroundOffset, 0 );
        direction = AtmosCubeDirection( uv, parameters.mFace );
    }
    else {
        CameraInfo camera = ( ( BufferReference<CameraInfo> )parameters.mCamera )[0];
        origin = AtmosClampGround( a, AtmosPlanetPosition( a, camera.mViewPosition.xyz ) );
        direction = AtmosSkyDirection( a, origin, uv );
    }

    AtmosIntegration result = AtmosIntegrate( a, origin, direction, a.mSunDirection.xyz, 1e6, kAtmosSkyIntegrationSteps, true, false, true );

    // The sun disk is not baked into IBL: direct sunlight belongs to the
    // directional light, and a sub-texel disk aliases during convolution.
    return float4( min( result.mRadiance * a.mSunIlluminance.xyz, float3( 60000 ) ), 1 );
}
