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

// GrassFlow-derived blade deformation, field variation and shading helpers.
// Copyright (c) 2025 Mithun Rajan. Adapted for Mikoto's terrain and wind field.
// https://github.com/Mithzzx/Project-GrassFlow
// MIT license: GrassFlow.LICENSE.txt in this directory.

// Adapted grass noise, bending and color primitives. Smooth world-space variation drives clumps
// and wind while blade coordinates control root-to-tip deformation.

#pragma once

float GrassFlowHash21( float2 p ) {
    p = frac( p * float2( 123.34, 456.21 ) );
    p += dot( p, p + 45.32 );
    return frac( p.x * p.y );
}

float GrassFlowNoise( float2 p ) {
    float2 cell = floor( p );
    float2 f = frac( p );
    float2 u = f * f * ( 3 - 2 * f );
    return lerp( lerp( GrassFlowHash21( cell ), GrassFlowHash21( cell + float2( 1, 0 ) ), u.x ),
        lerp( GrassFlowHash21( cell + float2( 0, 1 ) ), GrassFlowHash21( cell + 1 ), u.x ), u.y );
}

// Three fractal octaves provide the broad clump/wind field.
static const uint kGrassFlowOctaveCount = 3;

float GrassFlowFbm( float2 p ) {
    float value = 0;
    float amplitude = .5;
    for ( uint octave = 0; octave < kGrassFlowOctaveCount; ++octave ) {
        value += amplitude * ( GrassFlowNoise( p ) * 2 - 1 );
        amplitude *= .5;
        p *= 2;
    }
    return value;
}

float3 GrassFlowNormalize( float3 value, float3 fallback ) {
    return dot( value, value ) > 1e-8 ? normalize( value ) : fallback;
}

// GrassFlow constrains the upper control points before evaluating its cubic.
// The same curve/tangent serves generated ribbons and imported mesh vertices.
void GrassFlowCurve( float t, float height, float bend, float3 up, float3 facing,
    float3 wind, float flutter, out float3 point, out float3 tangent ) {
    float3 p1 = up * height * .33 + facing * height * bend * .1;
    float3 p2 = up * height * .66 + facing * height * bend * .4 + wind * height;
    float3 p3 = up * height + facing * height * bend + wind * height * 2.4;
    p2 = GrassFlowNormalize( p2, up ) * height * .66;
    p3 = GrassFlowNormalize( p3, up ) * height;
    float u = 1 - t;
    point = 3 * u * u * t * p1 + 3 * u * t * t * p2 + t * t * t * p3;
    float3 derivative = 3 * u * u * p1 + 6 * u * t * ( p2 - p1 ) + 3 * t * t * ( p3 - p2 );
    float3 flutterDirection = GrassFlowNormalize( cross( up, wind ), cross( up, facing ) );
    point += flutterDirection * flutter * height * t * t * t * t;
    derivative += flutterDirection * flutter * height * 4 * t * t * t;
    tangent = GrassFlowNormalize( derivative, up );
}

float3 GrassFlowColor( float t, float variation, float3 root, float3 middle, float3 tip,
    float3 dry, float dryness, float colorVariation ) {
    float3 healthy = t < .5 ? lerp( root, middle, t * 2 ) : lerp( middle, tip, ( t - .5 ) * 2 );
    float dryMask = saturate( variation * 2 - 1 + dryness );
    float3 color = lerp( healthy, dry, dryMask * dryness );
    float shift = ( variation - .5 ) * colorVariation;
    return lerp( color, color * float3( 1 + shift, 1, 1 - shift * .5 ), colorVariation );
}
