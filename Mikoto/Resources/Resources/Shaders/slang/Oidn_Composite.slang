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

// Copy the whole denoised HDR image into the display signal. Raw progressive
// history was saved before this pass, so filtering never feeds back into it.
#include "base/Base.slang"
#include "base/Memory_Helpers.slang"

struct Parameters {
    uint mFiltered;
    uint mOutput;
    uint mWidth;
    uint mHeight;
    float mSplit;
};
MKT_DECLARE_TEXTURE_SRV( Texture2D<float4> )
textures;
MKT_DECLARE_TEXTURE_UAV( RWTexture2D<float4> )
images;
MKT_DECLARE_PUSH_CONSTANT( Parameters, parameters );

[shader( "compute" )]
[numthreads( kImageGroupWidth, kImageGroupHeight, 1 )]
void main( uint3 id : SV_DispatchThreadID ) {
    if ( id.x >= parameters.mWidth || id.y >= parameters.mHeight ) {
        return;
    }
    // The left side remains raw when comparing the two images in the viewport.
    if ( ( float( id.x ) + .5 ) / float( parameters.mWidth ) < parameters.mSplit ) {
        return;
    }
    images[parameters.mOutput][id.xy] = textures[parameters.mFiltered].Load( int3( id.xy, 0 ) );
}
