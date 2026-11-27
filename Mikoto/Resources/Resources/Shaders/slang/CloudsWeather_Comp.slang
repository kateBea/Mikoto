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

// Generate the 2D weather controls used to vary procedural cloud coverage. Texel coordinates
// map to the weather domain independently of the camera.

#include "base/CloudNoise_Helpers.slang"
#include "Clouds_Common.slang"

// Independent regional coverage, form and cloud-top fields. All channels are
// periodic so the weather can move with a volume without texture-edge seams.
[shader( "compute" )]
[numthreads( kImageGroupWidth, kImageGroupHeight, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    if ( any( thread.xy >= 512 ) ) {
        return;
    }
    float2 uv = ( float2( thread.xy ) + .5 ) / 512;
    float2 warp = float2( CloudGradientNoise( float3( uv * 4, .37 ), 4 ),
                      CloudGradientNoise( float3( uv * 4 + 1.7, .83 ), 4 ) ) -
                  .5;
    float3 p = float3( uv + warp * .14, .31 );
    float coverage = saturate( ( CloudGradientFbm( p, 4 ) - .3 ) / .4 );
    float form = saturate( ( CloudGradientFbm( p + float3( .27, .61, .19 ), 2 ) - .3 ) / .4 );
    float height = saturate( ( CloudGradientFbm( p + float3( .71, .13, .53 ), 4 ) - .3 ) / .4 );
    float base = CloudGradientFbm( p + float3( .43, .29, .79 ), 2 );
    cloudOutputs[parameters.mOutput][thread.xy] = float4( coverage, form, height, base );
}
