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

// Resolve reconstructed HDR or the raw fallback at viewport resolution.
// Exposure, bloom, tone mapping and editor overlays run afterward.

#include "base/Base.slang"
#include "base/Memory_Helpers.slang"

struct Parameters {
    uint mSource;
    uint mOutput;
    uint mSampler;
    uint mWidth;
    uint mHeight;
};
MKT_DECLARE_PUSH_CONSTANT( Parameters, parameters );
MKT_DECLARE_TEXTURE_SRV( Texture2D<float4> ) textures;
MKT_DECLARE_TEXTURE_UAV( RWTexture2D<float4> ) outputs;
MKT_DECLARE_SAMPLERS samplers;

[shader( "compute" )]
[numthreads( kImageGroupWidth, kImageGroupHeight, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    if ( thread.x >= parameters.mWidth || thread.y >= parameters.mHeight ) {
        return;
    }
    float2 uv = ( float2( thread.xy ) + .5 ) / float2( parameters.mWidth, parameters.mHeight );
    float3 color = textures[parameters.mSource].SampleLevel( samplers[parameters.mSampler], uv, 0 ).rgb;
    outputs[parameters.mOutput][thread.xy] = float4( color, 1 );
}
