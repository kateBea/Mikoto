// MIT License
//
// Copyright (c) 2026 ケイト
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// GPU port of my CPU pathtracer available at:
// https://github.com/kateBea/RT-In-One-WeekEnd.

#include "raytracing/Camera.slang"

struct Parameters {
    Pointer mSpheres;
    uint mImageIndex;
    uint mSphereCount;
    uint mTileX;
    uint mTileY;
};

MKT_DECLARE_TEXTURE_UAV( RWTexture2D<float4> ) textures;
MKT_DECLARE_PUSH_CONSTANT( Parameters, parameters );

[shader( "compute" )]
[numthreads( 16, 16, 1 )]
void main( uint3 tid : SV_DispatchThreadID ) {
    RWTexture2D<float4> output = textures[parameters.mImageIndex];
    uint width;
    uint height;
    output.GetDimensions( width, height );
    uint2 pixel = tid.xy + uint2( parameters.mTileX, parameters.mTileY );
    if ( pixel.x >= width || pixel.y >= height ) {
        return;
    }

    // Tiled mode initializes the image before tracing its first tile.
    if ( parameters.mSphereCount == 0 ) {
        output[pixel] = float4( 0, 0, 0, 1 );
        return;
    }

    swrt::Camera camera = swrt::CreateCamera( width, height );
    uint state = pixel.y * width + pixel.x + 1;
    float3 color = float3( 0 );
    static const uint samplesPerPixel = 10u;

    for ( uint sample = 0; sample < samplesPerPixel; ++sample ) {
        swrt::Ray ray = swrt::GetRay( camera, pixel, state );
        color += swrt::RayColor( ray, parameters.mSpheres, parameters.mSphereCount, state );
    }
    // Original color.hh: average, gamma 2, then truncate into [0, 255].
    float3 gamma = sqrt( max( color * ( 1.0 / float( samplesPerPixel ) ), float3( 0 ) ) );
    float3 bytes = floor( 255.999 * clamp( gamma, 0.0, 0.999 ) );
    output[pixel] = float4( bytes / 255.0, 1 );
}
