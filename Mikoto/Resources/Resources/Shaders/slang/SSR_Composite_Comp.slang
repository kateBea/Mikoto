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

#include "base/SSR_Common.slang"

struct Parameters {
    Pointer Frame;
    uint Scene;
    uint Reflection;
    uint Environment;
    uint Output;
};

MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;
MKT_DECLARE_TEXTURE_UAV( RWTexture2D<float4> ) images;
MKT_DECLARE_PUSH_CONSTANT( Parameters, parameters );

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    SsrFrame frame = ( ( BufferReference<SsrFrame> )parameters.Frame )[0];
    uint2 pixel = thread.xy;
    if ( any( pixel >= uint2( frame.Screen.xy ) ) ) {
        return;
    }
    float4 scene = textures[parameters.Scene].Load( int3( pixel, 0 ) );
    float4 reflection = textures[parameters.Reflection].Load( int3( pixel, 0 ) );
    float3 environment = textures[parameters.Environment].Load( int3( pixel, 0 ) ).rgb;
    float coverage = saturate( reflection.w * frame.Filter.z );
    // Misses retain the exact existing environment lobe. A hit replaces it,
    // preserving direct lighting, diffuse IBL and emission in scene-linear HDR.
    images[parameters.Output][pixel] = float4( max( scene.rgb + coverage * ( reflection.rgb - environment ), 0 ), scene.a );
}
