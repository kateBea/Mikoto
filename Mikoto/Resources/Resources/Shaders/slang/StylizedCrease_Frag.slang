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

// Visible-surface creases complement the inverted-hull silhouette pass.

// Detect stylized crease edges from neighboring geometry guides. Depth and normal
// discontinuities select the line contribution over the lit surface.

#include "base/Memory_Helpers.slang"
#include "base/Camera_Helpers.slang"
// Eight radial directions sample the crease neighborhood.
static const uint kCreaseDirectionCount = 8;

struct Parameters {
    Pointer mCamera;
    uint mMask;
    uint mNormal;
    uint mPosition;
    uint mWidth;
    uint mHeight;
    uint mPadding;
};
MKT_DECLARE_PUSH_CONSTANT( Parameters, parameters );
MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_TEXTURE_SRV( Texture2D ) images;

[shader("fragment")]
float4 main( float2 unusedUV : TEXCOORD0, float4 screen : SV_Position ) : SV_Target0 {
    int2 pixel = int2( screen.xy );
    float4 mask = images[parameters.mMask].Load( int3( pixel, 0 ) );
    if ( mask.a <= 0 ) {
        discard;
    }
    float4 center = images[parameters.mPosition].Load( int3( pixel, 0 ) );
    if ( center.w <= 0 ) {
        discard;
    }
    float3 normal = normalize( images[parameters.mNormal].Load( int3( pixel, 0 ) ).xyz * 2 - 1 );
    CameraInfo camera = ( ( BufferReference<CameraInfo> )parameters.mCamera )[0];
    float3 viewNormal = normalize( mul( camera.mViewMatrix, float4( normal, 0 ) ).xyz );
    float width = mask.a * 12;
    float radius = max( 1.0, width * 0.5 );
    const float2 directions[kCreaseDirectionCount] = {
        float2(1,0), float2(-1,0), float2(0,1), float2(0,-1),
        float2(.7071,.7071), float2(-.7071,.7071), float2(.7071,-.7071), float2(-.7071,-.7071)
    };
    float edge = 0;
    for ( uint tap = 0; tap < kCreaseDirectionCount; ++tap ) {
        int2 q = pixel + int2( round( directions[tap] * radius ) );
        // The viewport boundary is not a mesh edge.
        if ( any( q < 0 ) || any( q >= int2(parameters.mWidth, parameters.mHeight) ) ) {
            continue;
        }
        float4 neighbor = images[parameters.mPosition].Load( int3( q, 0 ) );
        if ( neighbor.w <= 0 ) {
            edge = 1;
            continue;
        }
        float3 otherNormal = normalize( images[parameters.mNormal].Load( int3( q, 0 ) ).xyz * 2 - 1 );
        float normalEdge = smoothstep( 0.15, 0.35, 1 - dot( normal, otherNormal ) );
        // Compare distance from the receiver plane rather than raw depth delta:
        // sloping flat faces must not become dark merely because depth varies.
        float planeDistance = abs( dot( neighbor.xyz - center.xyz, viewNormal ) );
        float depthEdge = smoothstep( 0.005, 0.015, planeDistance / max( center.w, 0.001 ) );
        edge = max( edge, max( normalEdge, depthEdge ) );
    }
    if ( edge <= 0 ) {
        discard;
    }
    // Hardware alpha blending preserves HDR scene colour; no scene-colour copy.
    return float4( mask.rgb, edge * saturate( width ) );
}
