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

// Combine filtered screen-space reflections with the scene using material reflection factors.
// Missing screen-space information retains the existing lighting fallback.

#include "base/SSR_Common.slang"

struct Parameters {
    Pointer mFrame;
    uint mScene;
    uint mReflection;
    uint mEnvironment;
    uint mOutput;
};

MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;
MKT_DECLARE_TEXTURE_UAV( RWTexture2D<float4> ) images;
MKT_DECLARE_PUSH_CONSTANT( Parameters, parameters );

[shader( "compute" )]
[numthreads( kImageGroupWidth, kImageGroupHeight, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    SsrFrame frame = ( ( BufferReference<SsrFrame> )parameters.mFrame )[0];
    uint2 pixel = thread.xy;
    if ( any( pixel >= uint2( frame.mScreen.xy ) ) ) {
        return;
    }
    float4 scene = textures[parameters.mScene].Load( int3( pixel, 0 ) );
    float4 reflection = textures[parameters.mReflection].Load( int3( pixel, 0 ) );
    float3 environment = textures[parameters.mEnvironment].Load( int3( pixel, 0 ) ).rgb;
    float coverage = saturate( reflection.w * frame.mFilter.z );
    // Misses retain the exact existing environment lobe. A hit replaces it,
    // preserving direct lighting, diffuse IBL and emission in scene-linear HDR.
    images[parameters.mOutput][pixel] = float4( max( scene.rgb + coverage * ( reflection.rgb - environment ), 0 ), scene.a );
}
