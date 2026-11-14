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

#include "Fog_Common.slang"

// Front-to-back integration of lit froxels, using the homogeneous segment solution.
// https://bartwronski.com/wp-content/uploads/2014/08/bwronski_volumetric_fog_siggraph2014.pdf
// https://pbr-book.org/4ed/Volume_Scattering/Transmittance

[numthreads( 8, 8, 1 )] void main( uint3 thread : SV_DispatchThreadID ) {
    FogFrameData frame = LoadBufferValue<FogFrameData>( parameters.Frame, 0 );
    if ( any( thread.xy >= frame.Grid.xy ) ) {
        return;
    }
    float3 radiance = 0.0, transmittance = 1.0;
    for ( uint slice = 0; slice < frame.Grid.z; ++slice ) {
        uint2 pixel = FogAtlasPixel( uint3( thread.xy, slice ), frame.Grid );
        float3 source = textures[parameters.Lighting].Load( int3( pixel, 0 ) ).rgb;
        float3 extinction = textures[parameters.Extinction].Load( int3( pixel, 0 ) ).rgb;
        float step = FogSliceDistance( slice + 1, frame ) - FogSliceDistance( slice, frame );
        float3 segmentT = exp( -extinction * step );
        float3 integral;
        for ( uint channel = 0; channel < 3; ++channel ) {
            integral[channel] = extinction[channel] * step < 0.001 ? step * ( 1.0 - 0.5 * extinction[channel] * step ) : ( 1.0 - segmentT[channel] ) / extinction[channel];
        }
        radiance += transmittance * source * integral;
        transmittance *= segmentT;
        images[parameters.Radiance][pixel] = float4( min( radiance, 60000.0 ), 0.0 );
        images[parameters.Transmittance][pixel] = float4( transmittance, 1.0 );
    }
}
