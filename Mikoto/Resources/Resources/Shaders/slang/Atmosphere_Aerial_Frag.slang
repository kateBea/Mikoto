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

#pragma once

#include "Atmosphere_Common.slang"

struct AtmosAerialOutput {
    float4 mRadiance : SV_Target0;
    float4 mTransmittance : SV_Target1;
};

[shader( "fragment" )]
AtmosAerialOutput main( float2 uv : TEXCOORD0, float4 pixel : SV_Position ) {
    AtmosphereData a = Atmosphere();
    uint slice = min( uint( pixel.y ) / 32, 31u );
    float2 sliceUv = float2( pixel.x, pixel.y - slice * 32 ) / 32;

    CameraInfo camera = ( ( BufferReference<CameraInfo> )parameters.mCamera )[0];
    float3 origin, direction;
    AtmosCameraRay( camera, sliceUv, a, origin, direction );

    // Measure depth from where the ray enters the atmosphere. Keep the slice
    // budget for air, even when the camera is far outside the shell.
    float entry = max( 0, AtmosEntryDistance( a, origin, direction ) );
    float depth = float( slice ) / 31;
    depth = depth * depth * a.mGround.w;

    AtmosIntegration result =
            AtmosIntegrate( a, origin, direction, a.mSunDirection.xyz, entry + depth, 2 + slice, true, false, false );
    AtmosAerialOutput output;
    output.mRadiance = float4( min( result.mRadiance * a.mSunIlluminance.xyz, float3( 60000 ) ), 1 );
    output.mTransmittance = float4( result.mTransmittance, 1 );

    return output;
}
