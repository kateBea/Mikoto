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

// Screen/world projection, sampling sequences and rough-reflection weights for SSR. Normal
// encoding and depth conventions are shared by trace, resolve and temporal filtering.

#pragma once

#include "Base.slang"
#include "Memory_Helpers.slang"
#include "Camera_Helpers.slang"

// Stachowiak / Uludag, SIGGRAPH 2015. The normalized BRDF/PDF estimator is the
// important distinction from blurring mirror reflections after tracing.
// https://advances.realtimerendering.com/s2015/Stochastic%20Screen-Space%20Reflections.pptx
// Implementation references, with different depth/binding conventions:
// https://github.com/GPUOpen-Effects/FidelityFX-SSSR/blob/master/ffx-sssr/ffx_sssr.h
// https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/shaders/ssr_resolveCS.hlsl

static const uint SsrMaxMipCount = 16;

// Each frame reserves four reflection attempts; inverse-sequence evaluation has a fixed digit budget.
static const uint kSsrDirectionAttempts = 4;
static const uint kSsrRadicalInverseDigits = 16;

// A 3x3 current reflection neighborhood bounds temporal history.
static const int kSsrHistoryClampRadius = 1;

struct SsrFrame {
    float4x4 mInverseView;
    float4x4 mPreviousViewProjection;
    float4x4 mPreviousView;
    float4 mScreen; // Full width/height, trace width/height.
    float4 mTrace; // Maximum distance, thickness, maximum perceptual roughness, sampling bias.
    float4 mFilter; // History weight, spatial sample count, intensity, reserved.
    uint4 mControl; // Sequence index, history validity, reversed depth, maximum traversal steps.
};

float3 SsrWorldPosition( float3 viewPosition, SsrFrame frame ) {
    return mul( frame.mInverseView, float4( viewPosition, 1 ) ).xyz;
}

// Project a view-space point into screen coordinates and normalize depth to the SSR near-to-far
// convention. Traversal uses this convention for both camera depth modes.
float3 SsrProject( float3 viewPosition, CameraInfo camera, SsrFrame frame ) {
    float4 clip = mul( camera.mProjection, float4( viewPosition, 1 ) );
    float3 ndc = clip.xyz / clip.w;
    return float3( ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5, frame.mControl.z != 0 ? 1 - ndc.z : ndc.z );
}

float SsrViewDepth( float3 screenPosition, CameraInfo camera, SsrFrame frame ) {
    float depth = frame.mControl.z != 0 ? 1 - screenPosition.z : screenPosition.z;
    float4 view = mul( camera.mInverseProjection, float4( screenPosition.x * 2 - 1, 1 - screenPosition.y * 2, depth, 1 ) );
    return -view.z / view.w;
}

uint SsrHash( uint value ) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ ( value >> 16 );
}

float SsrRadicalInverse( uint value, uint base ) {
    float inverse = 1.0 / float( base );
    float result = 0;
    for ( uint digit = 0; digit < kSsrRadicalInverseDigits && value != 0; ++digit ) {
        result += float( value % base ) * inverse;
        value /= base;
        inverse /= float( base );
    }
    return result;
}

// Rotate a low-discrepancy sequence independently per pixel. The frame/attempt index advances
// samples while the spatial hash decorrelates neighboring pixels.
float2 SsrSequence( uint2 pixel, uint frame, uint attempt = 0 ) {
    uint seed = SsrHash( pixel.x + pixel.y * 65537u );
    float2 rotation = float2( seed & 65535u, seed >> 16 ) / 65536.0;
    uint index = ( frame * kSsrDirectionAttempts + attempt ) % 65536u + 1;
    return frac( float2( SsrRadicalInverse( index, 2 ), SsrRadicalInverse( index, 3 ) ) + rotation );
}

float SsrDistribution( float roughness, float normalHalf ) {
    float alpha = max( roughness * roughness, 0.002 );
    float a2 = alpha * alpha;
    float denominator = normalHalf * normalHalf * ( a2 - 1 ) + 1;
    return a2 / max( 3.14159265359 * denominator * denominator, 1e-12 );
}

float SsrSmithVisibility( float roughness, float normalView, float normalLight ) {
    float alpha = max( roughness * roughness, 0.002 );
    float a2 = alpha * alpha;
    float view = normalLight * sqrt( normalView * normalView * ( 1 - a2 ) + a2 );
    float light = normalView * sqrt( normalLight * normalLight * ( 1 - a2 ) + a2 );
    return 0.5 / max( view + light, 1e-6 );
}

// Evaluate the receiving pixel BRDF divided by the original ray PDF. Normalized spatial
// reconstruction uses this weight to reuse stochastic hits without a simple blur bias.
float SsrRayWeight( float3 normal, float3 view, float3 light, float roughness, float originalPdf ) {
    float normalLight = saturate( dot( normal, light ) );
    float normalView = max( saturate( dot( normal, view ) ), 0.001 );
    float3 halfSum = view + light;
    if ( normalLight <= 0 || dot( halfSum, halfSum ) < 1e-8 ) {
        return 0;
    }
    float3 halfVector = normalize( halfSum );
    float localBrdf = SsrDistribution( roughness, saturate( dot( normal, halfVector ) ) ) *
                      SsrSmithVisibility( roughness, normalView, normalLight ) * normalLight;
    return min( localBrdf / max( originalPdf, 1e-6 ), 1e6 );
}

// Octahedrally encode a unit normal into two channels. Folding the lower hemisphere preserves
// directional information for temporal guide storage.
float2 SsrEncodeNormal( float3 normal ) {
    normal /= abs( normal.x ) + abs( normal.y ) + abs( normal.z );
    float2 packed = normal.xy;
    if ( normal.z < 0 ) {
        packed = ( 1 - abs( packed.yx ) ) * float2( packed.x >= 0 ? 1 : -1, packed.y >= 0 ? 1 : -1 );
    }
    return packed;
}

// Unfold the octahedral normal representation and renormalize it before geometry-agreement
// tests.
float3 SsrDecodeNormal( float2 packed ) {
    float3 normal = float3( packed, 1 - abs( packed.x ) - abs( packed.y ) );
    if ( normal.z < 0 ) {
        normal.xy = ( 1 - abs( normal.yx ) ) * float2( normal.x >= 0 ? 1 : -1, normal.y >= 0 ? 1 : -1 );
    }
    return normalize( normal );
}

bool SsrInside( float2 uv ) {
    return all( uv > 0 ) && all( uv < 1 );
}
