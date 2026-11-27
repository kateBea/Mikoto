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

// Shared material-model selection, face orientation and direct-light BRDF evaluation. Stylized,
// unlit and coated surfaces branch here before their lighting terms are combined.

#pragma once

// Matches material::ShadingModel. Shared by the scene and thumbnail shaders.
static const uint SURFACE_CLEAR_COAT = 1;
static const uint SURFACE_TOON = 2;
static const uint SURFACE_FLAT = 3;
static const uint SURFACE_CELL = 4;
static const uint SURFACE_DIELECTRIC = 6;
static const uint SURFACE_CONDUCTOR = 7;
static const uint SURFACE_UNLIT = 8;

bool SurfaceStylized( uint model ) { return model == SURFACE_TOON || model == SURFACE_CELL; }
bool SurfaceRasterModel( uint model ) { return model >= SURFACE_CLEAR_COAT && model <= SURFACE_CELL; }

// Derivatives must be evaluated before discard, then oriented to the authored
// surface normal. This is independent of framebuffer Y and mirrored transforms.
float3 SurfaceFaceNormal( float3 dx, float3 dy, float3 authored ) {
    float3 n = cross( dx, dy );
    float lengthSquared = dot( n, n );
    if ( lengthSquared < 1e-20 ) { return normalize( authored ); }
    n *= rsqrt( lengthSquared );
    return dot( n, authored ) < 0 ? -n : n;
}

float SurfaceCoatFresnel( float cosine ) {
    return 0.04 + 0.96 * pow( 1.0 - saturate( cosine ), 5.0 );
}

// GGX clear layer, IOR 1.5. Attenuate the substrate on entry and exit.
// Layering reference: Khronos KHR_materials_clearcoat (not a glTF importer).
// https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_clearcoat
float3 SurfaceDirect( float3 baseLighting, float3 diffuse, float3 f0, float roughness,
                      float3 n, float3 v, float3 l, uint model, float4 settings, float bands, float visibility = 1.0,
                      float3 coatNormal = float3( 0 ), float lightWeight = 1.0 ) {
    float nl = saturate( dot( n, l ) );
    if ( model == SURFACE_CLEAR_COAT ) {
        n = dot( coatNormal, coatNormal ) > 0 ? coatNormal : n;
        nl = saturate( dot( n, l ) );
        float weight = saturate( settings.x );
        if ( weight == 0 ) { return baseLighting; }
        float nv = max( saturate( dot( n, v ) ), 0.001 );
        float3 sum = l + v;
        float3 h = sum * rsqrt( max( dot( sum, sum ), 1e-8 ) );
        float nh = saturate( dot( n, h ) );
        float a = max( settings.y * settings.y, 0.002025 );
        float a2 = a * a;
        float d = nh * nh * ( a2 - 1.0 ) + 1.0;
        float distribution = a2 / max( 3.14159265 * d * d, 1e-10 );
        float gv = 2 * nv / ( nv + sqrt( a2 + ( 1 - a2 ) * nv * nv ) );
        float gl = 2 * nl / max( nl + sqrt( a2 + ( 1 - a2 ) * nl * nl ), 1e-6 );
        float coat = weight * distribution * gv * gl * SurfaceCoatFresnel( dot( v, h ) ) / ( 4 * nv );
        float transmission = ( 1 - weight * SurfaceCoatFresnel( nl ) ) * ( 1 - weight * SurfaceCoatFresnel( nv ) );
        return baseLighting * transmission + coat * lightWeight;
    }
    if ( SurfaceStylized( model ) ) {
        // Quantize visible illumination together, rather than multiplying a
        // smooth PCF shadow over an already quantized lighting band.
        float visibleNl = nl * saturate( visibility );
        float ramp;
        if ( model == SURFACE_TOON ) {
            ramp = smoothstep( settings.z - settings.w, settings.z + settings.w, visibleNl );
        } else {
            float count = clamp( round( bands ), 2.0, 16.0 );
            ramp = min( floor( visibleNl * count ), count - 1 ) / ( count - 1 );
        }
        float3 sum = l + v;
        float nh = saturate( dot( n, sum * rsqrt( max( dot( sum, sum ), 1e-8 ) ) ) );
        // Rough materials may have a broader highlight, but not the same
        // full-strength white disc as glossy ones.
        float gloss = 1 - saturate( roughness );
        float highlight = smoothstep( 0.98 - roughness * 0.25, 0.99 - roughness * 0.25, nh ) * gloss * gloss;
        // No light leaks at the terminator, including threshold == 0.
        return visibleNl > 0 ? ( diffuse * ramp / 3.14159265 + f0 * highlight * ramp ) * lightWeight : float3( 0 );
    }
    return baseLighting;
}
