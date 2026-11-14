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

#pragma once
#include "SoftwarePathtracer.slang"

namespace swrt {
    struct Camera {
        float3 center;
        float3 pixel00;
        float3 pixelDeltaU;
        float3 pixelDeltaV;
        float3 defocusDiskU;
        float3 defocusDiskV;
    };

    Camera CreateCamera( uint width, uint height ) {
        Camera camera;
        camera.center = float3( 13, 2, 3 );
        float3 lookAt = float3( 0 );
        float3 up = float3( 0, 1, 0 );
        float focusDistance = 10;
        float viewportHeight = 2 * tan( radians( 20.0 ) * 0.5 ) * focusDistance;
        float viewportWidth = viewportHeight * float( width ) / float( height );
        float3 w = normalize( camera.center - lookAt );
        float3 u = normalize( cross( up, w ) );
        float3 v = cross( w, u );
        float3 viewportU = viewportWidth * u;
        float3 viewportV = -viewportHeight * v;
        camera.pixelDeltaU = viewportU / float( width );
        camera.pixelDeltaV = viewportV / float( height );
        camera.pixel00 = camera.center - focusDistance * w - 0.5 * ( viewportU + viewportV ) +
                         0.5 * ( camera.pixelDeltaU + camera.pixelDeltaV );
        float defocusRadius = focusDistance * tan( radians( 0.6 ) * 0.5 );
        camera.defocusDiskU = defocusRadius * u;
        camera.defocusDiskV = defocusRadius * v;
        return camera;
    }

    Ray GetRay( Camera camera, uint2 pixel, inout uint state ) {
        float2 jitter;
        jitter.x = Random( state ) - 0.5;
        jitter.y = Random( state ) - 0.5;
        float3 sample = camera.pixel00 + ( float( pixel.x ) + jitter.x ) * camera.pixelDeltaU +
                        ( float( pixel.y ) + jitter.y ) * camera.pixelDeltaV;
        float2 disk = RandomInUnitDisk( state );
        Ray ray;
        ray.origin = camera.center + disk.x * camera.defocusDiskU + disk.y * camera.defocusDiskV;
        ray.direction = sample - ray.origin;
        return ray;
    }
}
