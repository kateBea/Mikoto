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

// Construct the tangent-space height footprint for bump and parallax mapping. Height gradients
// perturb shading normals while view direction controls the UV displacement.

#pragma once
// PBRT: displace differential tangents, then reconstruct the shading normal.
// https://pbr-book.org/4ed/Textures_and_Materials/Material_Interface_and_Implementations#BumpMapping
// Recover world-space surface tangents from position and UV derivatives. Degenerate UV mappings
// cannot provide a stable frame for parallax or bump evaluation.
bool HeightTangents( float3 dp1, float3 dp2, float2 duv1, float2 duv2, out float3 dpdu, out float3 dpdv ) {
    float determinant = duv1.x * duv2.y - duv1.y * duv2.x;
    dpdu = dpdv = 0;
    if ( abs( determinant ) < 1e-10 ) {
        return false;
    }
    dpdu = ( dp1 * duv2.y - dp2 * duv1.y ) / determinant;
    dpdv = ( dp2 * duv1.x - dp1 * duv2.x ) / determinant;
    return dot( dpdu, dpdu ) > 1e-12 && dot( dpdv, dpdv ) > 1e-12;
}
// Convert height texture gradients into a perturbed world-space normal. The derivative frame
// determines the physical direction of each texture-space slope.
float3 HeightBumpNormal( Texture2D map, SamplerState sampler, float2 uv,
    float3 normal, float3 dpdu, float3 dpdv, float scale ) {
    uint width;
    uint height;
    map.GetDimensions( width, height );
    float2 step = 1.0 / float2( width, height );
    float h = map.SampleLevel( sampler, uv, 0 ).r;
    float dhdu = ( map.SampleLevel( sampler, uv + float2( step.x, 0 ), 0 ).r - h ) * scale / step.x;
    float dhdv = ( map.SampleLevel( sampler, uv + float2( 0, step.y ), 0 ).r - h ) * scale / step.y;
    float3 bumped = cross( dpdu + normal * dhdu, dpdv + normal * dhdv );
    if ( dot( bumped, bumped ) < 1e-12 ) {
        return normal;
    }
    return normalize( bumped ) * ( dot( bumped, normal ) < 0 ? -1.0 : 1.0 );
}
// ATI's bounded height-field march, then interpolate the crossing interval.
// https://advances.realtimerendering.com/s2006/Tatarchuk-POM.pdf
// Offset the material UV along the view direction through the height field. This changes
// sampling coordinates while leaving rasterized geometry/depth unchanged.
float2 HeightParallax( Texture2D map, SamplerState sampler, float2 uv, float3 toEye,
    float3 normal, float3 dpdu, float3 dpdv, float scale, float midlevel, uint maximumSteps ) {
    float3 t = normalize( dpdu - normal * dot( dpdu, normal ) );
    float3 b = normalize( cross( normal, t ) ) * sign( dot( cross( dpdu, dpdv ), normal ) );
    float3 view = float3( dot( toEye, t ), dot( toEye, b ), abs( dot( toEye, normal ) ) );
    if ( view.z < 0.02 ) {
        return uv;
    }
    uint steps = uint( lerp( float( maximumSteps ), 8.0, saturate( view.z ) ) );
    float2 delta = view.xy / max( view.z, 0.02 ) * scale;
    float2 current = uv + delta * ( 1 - midlevel );
    float2 stepUV = delta / float( steps );
    float layer = 0;
    float depth = 1 - map.SampleLevel( sampler, current, 0 ).r;
    for ( uint i = 0; i < steps && layer < depth; ++i ) {
        current -= stepUV;
        layer += 1.0 / float( steps );
        depth = 1 - map.SampleLevel( sampler, current, 0 ).r;
    }
    float after = depth - layer;
    float before = 1 - map.SampleLevel( sampler, current + stepUV, 0 ).r - ( layer - 1.0 / float( steps ) );
    float weight = abs( after - before ) > 1e-6 ? saturate( after / ( after - before ) ) : 0;
    return current + stepUV * weight;
}
