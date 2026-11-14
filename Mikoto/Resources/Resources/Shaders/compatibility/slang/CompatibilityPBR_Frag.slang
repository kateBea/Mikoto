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

#include "base/CompatibilitySurface.slang"
#include "base/CompatibilityLighting.slang"
#include "base/CompatibilityIBL.slang"

#include "../../slang/base/SurfaceShading_Helpers.slang"
static const float kDielectricF0 = 0.04;

[[vk::binding( 5, 0 )]] Texture2D metallicRoughness : register( t5 );
[[vk::binding( 6, 0 )]] Texture2D normalMap : register( t6 );
[[vk::binding( 7, 0 )]] Texture2D occlusion : register( t7 );
[[vk::binding( 8, 0 )]] Texture2D emissive : register( t8 );
[[vk::binding( 16, 0 )]] Texture2D<float> shadowMap : register( t16 );

// Compare the receiver depth against the nearest caster, with a 3x3 PCF kernel.
// https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping
float ShadowVisibility( float3 worldPosition, float3 normal, float3 lightDirection ) {
    float3 position = mul( frame.ShadowViewProjection, float4( worldPosition, 1.0 ) ).xyz;
    if ( any( abs( position.xy ) >= 1.0 ) || position.z <= 0.0 || position.z >= 1.0 ) {
        return 1.0;
    }

#if defined( MKT_SUPPORT_D3D11 ) || defined( MKT_SUPPORT_D3D12 )
    position.y = -position.y;
#endif
    uint width, height;
    shadowMap.GetDimensions( width, height );
    int2 extent = int2( width, height );
    int2 pixel = int2( ( position.xy * 0.5 + 0.5 ) * float2( extent ) );
    static const float kMinBias = 0.0002;
    static const float kSlopeBias = 0.001;
    float depth = position.z - max( kMinBias, kSlopeBias * ( 1.0 - saturate( dot( normal, -lightDirection ) ) ) );
    float visible = 0.0;
    for ( int y = -1; y <= 1; ++y ) {
        for ( int x = -1; x <= 1; ++x ) {
            int2 tap = clamp( pixel + int2( x, y ), int2( 0 ), extent - 1 );
            visible += depth <= shadowMap.Load( int3( tap, 0 ) ) ? 1.0 : 0.0;
        }
    }

    return visible / 9.0;
}

float3 SurfaceNormal( SurfaceInput input ) {
    float3 normal = normalize( input.Normal );
    if ( draw.Surface.z == 0.0 ) {
        return normal;
    }

    // Recover a tangent basis from the packed mesh's UVs and world position.
    // https://www.thetenthplanet.de/archives/1180
    float3 dpdx = ddx( input.WorldPosition );
    float3 dpdy = ddy( input.WorldPosition );
    float2 uv = SurfaceUV( input, draw.UVSets.z );
    float2 dtdx = ddx( uv );
    float2 dtdy = ddy( uv );

    float3 crossX = cross( normal, dpdx );
    float3 crossY = cross( dpdy, normal );
    float3 tangent = crossY * dtdx.x + crossX * dtdy.x;
    float3 bitangent = crossY * dtdx.y + crossX * dtdy.y;
    float scale = rsqrt( max( max( dot( tangent, tangent ), dot( bitangent, bitangent ) ), kEpsilon ) );

    float3 sampleNormal = normalMap.Sample( materialSampler, uv ).xyz * 2.0 - 1.0;
    sampleNormal.xy *= draw.Surface.z;

    return normalize( tangent * scale * sampleNormal.x + bitangent * scale * sampleNormal.y + normal * sampleNormal.z );
}

[shader( "fragment" )]
float4 main( SurfaceInput input, bool rasterFrontFace : SV_IsFrontFace ) : SV_Target {
    // Same material inputs as PBR_MetallicRoughness_Frag.slang, using fixed slots.
    float4 base = baseColor.Sample( materialSampler, SurfaceUV( input, draw.UVSets.x ) );
    if ( ( draw.Material.w & kBaseColorSRGB ) == 0u ) {
        base.rgb = pow( max( base.rgb, 0.0 ), 2.2 );
    }

    base *= draw.BaseColor * input.Color;
    float3 normal = SurfaceNormal( input );
    float2 mr = metallicRoughness.Sample( materialSampler, SurfaceUV( input, draw.UVSets.y ) ).bg;
    float ao = occlusion.Sample( materialSampler, SurfaceUV( input, draw.UVSets.w ) ).r;
    float3 emission = emissive.Sample( materialSampler, SurfaceUV( input, draw.Material.x ) ).rgb;
    if ( ( draw.Material.w & kEmissiveSRGB ) == 0u ) {
        emission = pow( max( emission, 0.0 ), 2.2 );
    }
    emission *= draw.Emission.rgb;

    // Evaluate texture derivatives before discarding coverage or backfaces.
    bool front = SurfaceFrontFacing( rasterFrontFace );
    float alpha = SurfaceAlpha( base.a );
    bool translucent = IsTranslucentBlend( alpha, draw.Object.z == kAlphaBlend );
    if ( translucent != ( draw.Object.w != 0u ) ) {
        discard;
    }

    alpha = translucent ? alpha : 1.0;
    if ( draw.Material.z == SURFACE_UNLIT ) {
        return float4( EncodeSceneColor( base.rgb + emission ).rgb, alpha );
    }

    normal = front ? normal : -normal;
    float metallic = saturate( mr.x * draw.Surface.x );
    float roughness = clamp( mr.y * draw.Surface.y, kMinimumRoughness, 1.0 );
    float3 view = normalize( frame.CameraPosition.xyz - input.WorldPosition );
    float3 diffuse = base.rgb * ( 1.0 - kDielectricF0 ) * ( 1.0 - metallic );
    float3 specular = lerp( float3( kDielectricF0 ), base.rgb, metallic );

    float3 color = 0.0;
    for ( uint i = 0; i < frame.Lighting.x; ++i ) {
        Light light = frame.Lights[i];
        float visibility = i + 1 == frame.Lighting.y ? ShadowVisibility( input.WorldPosition, normal, light.DirectionRadius.xyz ) : 1.0;
        color += visibility * DirectLight( light, input.WorldPosition, normal, view, diffuse, specular, metallic, roughness );
    }

    if ( frame.Environment.x > 0.0 && frame.Environment.w != 0.0 ) {
        float nDotV = saturate( dot( normal, view ) );
        float3 lighting = GetCompatibilityIBL( normal, reflect( -view, normal ), nDotV, roughness, diffuse, specular, frame.Environment.w );
        color += lighting * lerp( 1.0, ao, draw.Emission.w ) * frame.Environment.x;
    }

    return float4( EncodeSceneColor( color + emission ).rgb, alpha );
}
