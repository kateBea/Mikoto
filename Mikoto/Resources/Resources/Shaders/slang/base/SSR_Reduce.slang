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

#include "SSR_Common.slang"

struct Parameters {
    Pointer Frame;
    Pointer Camera;
    uint Input;
    uint Output;
    uint Width;
    uint Height;
    uint Mode; // 0: depth from G-buffer, 1: minimum depth, 2: HDR colour average.
};

MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;
#if defined( MKT_SSR_DEPTH )
MKT_DECLARE_TEXTURE_UAV( RWTexture2D<float> ) images;
#else
MKT_DECLARE_TEXTURE_UAV( RWTexture2D<float4> ) images;
#endif
MKT_DECLARE_PUSH_CONSTANT( Parameters, parameters );

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    uint2 pixel = thread.xy;
    if ( pixel.x >= parameters.Width || pixel.y >= parameters.Height ) {
        return;
    }

#if defined( MKT_SSR_DEPTH )
    if ( parameters.Mode == 0 ) {
        SsrFrame frame = ( ( BufferReference<SsrFrame> )parameters.Frame )[0];
        CameraInfo camera = ( ( BufferReference<CameraInfo> )parameters.Camera )[0];
        float4 position = textures[parameters.Input].Load( int3( pixel, 0 ) );
        images[parameters.Output][pixel] = position.w > 0 ? saturate( SsrProject( position.xyz, camera, frame ).z ) : 1;
        return;
    }
#endif

    uint width, height;
    textures[parameters.Input].GetDimensions( width, height );
    float4 value = parameters.Mode == 1 ? float4( 1 ) : float4( 0 );
    uint samples = 0;

    // Overlap the source footprint at odd dimensions. A min-depth cell must
    // conservatively cover its entire normalized screen region for traversal.
    float2 scale = float2( width, height ) / float2( parameters.Width, parameters.Height );
    uint2 first = uint2( floor( float2( pixel ) * scale ) );
    uint2 last = min( uint2( ceil( float2( pixel + 1 ) * scale ) ), uint2( width, height ) );
    for ( uint y = first.y; y < last.y; ++y ) {
        for ( uint x = first.x; x < last.x; ++x ) {
            float4 sample = textures[parameters.Input].Load( int3( x, y, 0 ) );
            value = parameters.Mode == 1 ? min( value, sample ) : value + sample;
            ++samples;
        }
    }

#if defined( MKT_SSR_DEPTH )
    images[parameters.Output][pixel] = value.r;
#else
    images[parameters.Output][pixel] = value / max( samples, 1u );
#endif
}
