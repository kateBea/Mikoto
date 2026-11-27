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

// Evaluate the configured vignette shape in screen space. The mask separates the geometric
// falloff from the later scene-color composition.

#include "base/Base.slang"

struct FSInput {
    float2 mUV : TEXCOORD0;
};

struct DrawParams {
    float2 mCenter;
    float2 mHalfSize;
    float mAspectRatio;
    float mFeather;
    uint mShape;
    uint mPadding;
};

MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );

float SignedDistanceBox( float2 p, float2 halfSize ) {
    float2 q = abs( p ) - halfSize;
    return length( max( q, 0.0 ) ) + min( max( q.x, q.y ), 0.0 );
}

float EvaluateVignette( float2 uv, float2 center, float2 halfSize, float aspectRatio, float feather, uint shape ) {
    float2 p = ( uv - center ) * 2.0;

    // Use the render target's aspect ratio so a circle stays round on a wide viewport.
    p.x *= aspectRatio;

    float distanceToEdge;
    uint shapeCircle = 0;
    if ( shape == shapeCircle ) {
        distanceToEdge = length( p ) - halfSize.x;
    } else {
        // Rectangle
        distanceToEdge = SignedDistanceBox( p, halfSize );
    }

    float width = max( feather, 0.0001 );

    // Interior = 0; exterior = 1. Feather spans both sides of the shape boundary.
    return smoothstep( -width, width, distanceToEdge );
}

[shader( "fragment" )]
float main( FSInput input ) : SV_Target0 {
    return EvaluateVignette(
        input.mUV,
        parameters.mCenter,
        parameters.mHalfSize,
        parameters.mAspectRatio,
        parameters.mFeather,
        parameters.mShape );
}
