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

// Fixed demo-camera basis and lens-ray construction for the software sphere tracer. Image size
// determines the pixel footprint; independent pixel/lens jitter samples antialiasing and depth
// of field.

#pragma once
#include "SoftwarePathtracer.slang"

namespace swrt {
    struct Camera {
        float3 mCenter;
        float3 mPixel00;
        float3 mPixelDeltaU;
        float3 mPixelDeltaV;
        float3 mDefocusDiskU;
        float3 mDefocusDiskV;
    };

    Camera CreateCamera( uint width, uint height ) {
        Camera camera;
        camera.mCenter = float3( 13, 2, 3 );
        float3 lookAt = float3( 0 );
        float3 up = float3( 0, 1, 0 );
        static const float focusDistance = 10.0;
        static const float verticalFov = 20.0;
        static const float defocusAngle = 0.6;

        float viewportHeight = 2 * tan( radians( verticalFov ) * 0.5 ) * focusDistance;
        float viewportWidth = viewportHeight * float( width ) / float( height );
        float3 w = normalize( camera.mCenter - lookAt );
        float3 u = normalize( cross( up, w ) );
        float3 v = cross( w, u );
        float3 viewportU = viewportWidth * u;
        float3 viewportV = -viewportHeight * v;
        camera.mPixelDeltaU = viewportU / float( width );
        camera.mPixelDeltaV = viewportV / float( height );
        camera.mPixel00 = camera.mCenter - focusDistance * w - 0.5 * ( viewportU + viewportV ) +
                         0.5 * ( camera.mPixelDeltaU + camera.mPixelDeltaV );
        float defocusRadius = focusDistance * tan( radians( defocusAngle ) * 0.5 );
        camera.mDefocusDiskU = defocusRadius * u;
        camera.mDefocusDiskV = defocusRadius * v;
        return camera;
    }

    Ray GetRay( Camera camera, uint2 pixel, inout uint state ) {
        float2 jitter;
        jitter.x = Random( state ) - 0.5;
        jitter.y = Random( state ) - 0.5;
        float3 sample = camera.mPixel00 + ( float( pixel.x ) + jitter.x ) * camera.mPixelDeltaU +
                        ( float( pixel.y ) + jitter.y ) * camera.mPixelDeltaV;

        // Jitter the image sample and the lens independently for depth of field.
        float2 disk = RandomInUnitDisk( state );
        Ray ray;
        ray.mOrigin = camera.mCenter + disk.x * camera.mDefocusDiskU + disk.y * camera.mDefocusDiskV;
        ray.mDirection = sample - ray.mOrigin;
        return ray;
    }
}
