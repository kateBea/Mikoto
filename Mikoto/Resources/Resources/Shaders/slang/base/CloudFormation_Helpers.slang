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

// Seeded cloud formation from smooth unions of analytic lobes and segments. Signed distances
// define the coarse shape before noise/detail modulates density.

#pragma once

// Seeded primitive clusters follow the modeling structure demonstrated by Cumulus.
// Independent implementation for Mikoto's world metric, weather and bindless fields.
// https://github.com/rubenaryo/Cumulus
uint CloudFormationHash( uint value ) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ ( value >> 16 );
}

float CloudFormationRandom( uint value ) {
    return float( CloudFormationHash( value ) >> 8 ) / 16777216.0;
}

// Blend two signed-distance lobes over a finite smoothing radius. The softened union joins
// billows without a hard intersection crease.
float CloudSmoothUnion( float a, float b, float radius ) {
    float blend = saturate( .5 + .5 * ( b - a ) / radius );
    return lerp( b, a, blend ) - radius * blend * ( 1 - blend );
}

float CloudEllipsoidDistance( float3 p, float3 radius ) {
    float a = length( p / radius );
    float b = length( p / ( radius * radius ) );
    return b > 1e-6 ? a * ( a - 1 ) / b : -min( radius.x, min( radius.y, radius.z ) );
}

float CloudTaperedSegmentDistance( float3 p, float3 start, float3 end, float bottomRadius, float topRadius ) {
    float3 axis = end - start;
    float t = saturate( dot( p - start, axis ) / max( dot( axis, axis ), 1e-6 ) );
    return length( p - lerp( start, end, t ) ) - lerp( bottomRadius, topRadius, t );
}

// Reconstruct the deterministic analytic formation from its seed. The same cloud parameters
// produce the same coarse silhouette for modeling and density evaluation.
float CloudFormationDistance( float3 p, float2 center, float radius, float height,
    float form, uint seed, float3 random ) {
    float thickness = lerp( .10, .44, saturate( form * .5 ) );
    thickness = lerp( thickness, .09, saturate( form - 2 ) );
    // A horizontal tapered body with uneven rounded lobes follows the
    // seeded cone/vesica construction, while form controls the flattening.
    float angle = random.z * 6.2831853;
    float3 axis = float3( cos( angle ), 0, sin( angle ) );
    float3 origin = float3( center.x, height, center.y );
    float vertical = clamp( thickness / .36, .2, 1.2 );
    float3 query = p - origin;
    query.y /= vertical;
    float body = CloudTaperedSegmentDistance( query, -axis * radius * .6,
        axis * radius * .6, radius * .65, radius * .48 ) *
        min( vertical, 1 );
    for ( uint lobe = 0; lobe < 5; ++lobe ) {
        float lobeAngle = 6.2831853 * ( float( lobe ) / 5 + random.x );
        float lobeRandom = CloudFormationRandom( seed + 31u + lobe * 7u );
        float3 centerLobe = origin + float3( cos( lobeAngle ) * radius * .8,
            radius * ( -.1 + lobeRandom * .75 ) * vertical, sin( lobeAngle ) * radius * .8 );
        float lobeRadius = radius * lerp( .42, .8, lobeRandom );
        body = CloudSmoothUnion( body, CloudEllipsoidDistance( p - centerLobe, float3( lobeRadius, lobeRadius * vertical, lobeRadius ) ), radius * .12 );
    }
    body = max( body, height - radius * .85 * vertical - p.y );
    return body;
}
