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

#pragma once

#include "base/Thumbnail_Helpers.slang"
#include "base/SurfaceShading_Helpers.slang"
#include "base/Bloom_Helpers.slang"

struct PreviewOutput {
    float4 mColor : SV_Target0;
    float4 mBloom : SV_Target1;
};

PreviewOutput PreviewResult( float4 color, float3 emission ) {
    PreviewOutput output;
    output.mColor = float4( BloomClampColor( color.rgb ), color.a );
    output.mBloom = float4( BloomClampColor( emission ), color.a );
    return output;
}

float PreviewVisibility( PreviewScene scene, float3 position, float3 normal ) {
    if ( scene.mLightDirection.w == 0 ) {
        return 1.0;
    }
    float4 projected = mul( scene.mLightViewProjection, float4( position, 1 ) );
    float3 coordinate = projected.xyz / projected.w;
    // Shadow depth is a top-left texture, while the light projection uses +Y up.
    float2 uv = coordinate.xy * float2( 0.5, -0.5 ) + 0.5;
    if ( any( uv < 0 ) || any( uv > 1 ) || coordinate.z <= 0 || coordinate.z >= 1 ) {
        return 1.0;
    }
    float bias = 0.0008 + 0.0015 * ( 1.0 - saturate( dot( normal, scene.mLightDirection.xyz ) ) );
    float visible = 0;
    // Compare each tap before averaging. Filtering raw depth would produce
    // false intermediate blockers at silhouette edges.
    int2 pixel = int2( uv * 1024.0 );
    for ( int y = -1; y <= 1; ++y ) {
        for ( int x = -1; x <= 1; ++x ) {
            float depth = previewTextures[preview.mShadow]
                                  .Load( int3( clamp( pixel + int2( x, y ), int2( 0 ), int2( 1023 ) ), 0 ) )
                                  .r;
            visible += coordinate.z - bias <= depth ? 1.0 : 0.0;
        }
    }
    return visible / 9.0;
}

[shader( "fragment" )]
PreviewOutput main( PreviewVertex input, bool front : SV_IsFrontFace ) {
    PreviewScene scene = ( ( BufferReference<PreviewScene> )preview.mScene )[0];
    PreviewDraw draw = ( ( BufferReference<PreviewDraw> )preview.mDraws )[preview.mDrawIndex];
    float3 flatNormal = SurfaceFaceNormal( ddx( input.mWorld ), ddy( input.mWorld ), input.mNormal );
    uint model = draw.mUv1.z == 0 && draw.mFlags.w < 0.5 ? uint( draw.mShadingModel.x ) : 0;
    bool facing = front != ( draw.mUv1.y != 0 );
    if ( !facing && draw.mFlags.z < 0.5 && draw.mFlags.w < 0.5 ) {
        discard;
    }
    float4 base = PreviewBaseColor( draw, input );
    if ( draw.mFlags.x == 1.0 && base.a < draw.mFlags.y ) {
        discard;
    }
    float3 normal = normalize( input.mNormal );
    if ( draw.mUv1.z == 1 ) {
        return PreviewResult( base, float3( 0 ) );
    }
    if ( draw.mUv1.z == 2 || draw.mUv1.z == 3 ) {
        return PreviewResult( float4( PreviewSkyColor( draw, normal ), 1 ), float3( 0 ) );
    }
    float3 view = normalize( scene.mCamera.xyz - input.mWorld );
    if ( draw.mFlags.z > 0.5 && dot( normal, view ) < 0 ) {
        normal = -normal;
    }
    float3 coatNormal = normal;
    if ( model == SURFACE_CLEAR_COAT && draw.mClearCoatNormalIndex >= 0 ) {
        float2 uv = PreviewUv( draw, input, draw.mClearCoatNormalTextureSet );
        float3 dp1 = ddx( input.mWorld );
        float3 dp2 = ddy( input.mWorld );
        float2 duv1 = ddx( uv );
        float2 duv2 = ddy( uv );
        float3 dp2perp = cross( dp2, coatNormal );
        float3 dp1perp = cross( coatNormal, dp1 );
        float3 tangent = dp2perp * duv1.x + dp1perp * duv2.x;
        float3 bitangent = dp2perp * duv1.y + dp1perp * duv2.y;
        float lengthSquared = max( dot( tangent, tangent ), dot( bitangent, bitangent ) );
        if ( lengthSquared > 1e-12 ) {
            float3 mapped = PreviewTexture( draw.mClearCoatNormalIndex, uv, float4( 0.5, 0.5, 1, 1 ) ).xyz * 2.0 - 1.0;
            mapped.xy *= draw.mClearCoatNormalScale;
            float inverseLength = rsqrt( lengthSquared );
            coatNormal = normalize( tangent * inverseLength * mapped.x + bitangent * inverseLength * mapped.y +
                                coatNormal * mapped.z );
        }
    }
    if ( model != SURFACE_FLAT && draw.mTextures0.y >= 0 ) {
        float2 uv = PreviewUv( draw, input, draw.mUv0.y );
        float3 dp1 = ddx( input.mWorld );
        float3 dp2 = ddy( input.mWorld );
        float2 duv1 = ddx( uv );
        float2 duv2 = ddy( uv );
        float3 dp2perp = cross( dp2, normal );
        float3 dp1perp = cross( normal, dp1 );
        float3 tangent = dp2perp * duv1.x + dp1perp * duv2.x;
        float3 bitangent = dp2perp * duv1.y + dp1perp * duv2.y;
        float lengthSquared = max( dot( tangent, tangent ), dot( bitangent, bitangent ) );
        if ( lengthSquared > 1e-12 ) {
            float3 mapped = PreviewTexture( draw.mTextures0.y, uv, float4( 0.5, 0.5, 1, 1 ) ).xyz * 2.0 - 1.0;
            mapped.xy *= draw.mFactors.z;
            float inverseLength = rsqrt( lengthSquared );
            normal = normalize( tangent * inverseLength * mapped.x + bitangent * inverseLength * mapped.y +
                                normal * mapped.z );
        }
    }
    if ( model == SURFACE_FLAT ) {
        normal = dot( flatNormal, view ) < 0 && draw.mFlags.z > 0.5 ? -flatNormal : flatNormal;
    }
    float4 packed = PreviewTexture( draw.mTextures0.z, PreviewUv( draw, input, draw.mUv0.z ), float4( 1 ) );
    float metallic = saturate( draw.mFactors.x * packed.b *
                               PreviewTexture( draw.mTextures1.y, PreviewUv( draw, input, draw.mUv0.z ), float4( 1 ) ).r );
    float roughness =
            clamp( draw.mFactors.y * packed.g *
                           PreviewTexture( draw.mTextures1.z, PreviewUv( draw, input, draw.mUv0.z ), float4( 1 ) ).r,
                   0.045, 1.0 );
    float ao = lerp( 1.0, PreviewTexture( draw.mTextures1.x, PreviewUv( draw, input, draw.mUv1.x ), float4( 1 ) ).r,
                     saturate( draw.mFactors.w ) );
    float3 light = scene.mLightDirection.xyz;
    float3 halfVector = normalize( view + light );
    float nv = max( dot( normal, view ), 0.001 );
    float nl = saturate( dot( normal, light ) );
    float nh = saturate( dot( normal, halfVector ) );
    float vh = saturate( dot( view, halfVector ) );
    float3 f0 = lerp( float3( 0.04 ), base.rgb, metallic );
    float transmission = 0;
    if ( model == SURFACE_DIELECTRIC ) {
        float ior = max( draw.mSurfaceShading.x, 1.0 );
        float ratio = (ior - 1) / (ior + 1);
        f0 = float3( ratio * ratio );
        metallic = 0;
        transmission = saturate( draw.mSurfaceShading.y );
    }
    float3 fresnel = f0 + ( 1.0 - f0 ) * pow( 1.0 - vh, 5.0 );
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denominator = nh * nh * ( alpha2 - 1.0 ) + 1.0;
    float distribution = alpha2 / max( 3.14159265 * denominator * denominator, 1e-7 );
    float k = ( roughness + 1.0 ) * ( roughness + 1.0 ) / 8.0;
    float geometry = nv / ( nv * ( 1.0 - k ) + k ) * nl / max( nl * ( 1.0 - k ) + k, 1e-6 );
    float3 specular = distribution * geometry * fresnel / max( 4.0 * nv * nl, 0.001 );
    float3 diffuse = ( 1.0 - fresnel ) * ( 1.0 - metallic ) * (1 - transmission) * base.rgb / 3.14159265;
    float visibility = PreviewVisibility( scene, input.mWorld, normal );
    float3 direct = SurfaceDirect( ( diffuse + specular ) * nl, ( 1 - metallic ) * base.rgb * 0.96,
                                  f0, roughness, normal, view, light, model, draw.mSurfaceShading, draw.mShadingModel.y, SurfaceStylized( model ) ? visibility : 1.0, coatNormal ) *
                    scene.mLightColor.rgb * scene.mLightColor.w *
                    ( SurfaceStylized( model ) ? 1.0 : visibility );
    float3 ambientFresnel = f0 + ( max( float3( 1.0 - roughness ), f0 ) - f0 ) * pow( 1.0 - nv, 5.0 );
    float3 reflection = PreviewBlurredEnvironment( reflect( -view, normal ), roughness );
    float3 ambient =
            ( ( 1.0 - metallic ) * (1 - transmission) * ( 1.0 - ambientFresnel ) * base.rgb * PreviewBlurredEnvironment( normal, 1.0 ) +
              reflection * ambientFresnel ) *
            scene.mSettings.z * ao;
    if ( SurfaceStylized( model ) ) {
        ambient = ( 1 - metallic ) * base.rgb * 0.96 * PreviewBlurredEnvironment( normal, 1.0 ) * scene.mSettings.z * ao;
    } else if ( model == SURFACE_CLEAR_COAT && draw.mSurfaceShading.x > 0 ) {
        float weight = draw.mSurfaceShading.x;
        float f = SurfaceCoatFresnel( max( dot( coatNormal, view ), 0.001 ) );
        ambient = ambient * ( 1 - weight * f ) + weight * f *
            PreviewBlurredEnvironment( reflect( -view, coatNormal ), draw.mSurfaceShading.y ) * scene.mSettings.z * ao;
    }
    float3 emission = draw.mUv1.z == 4
                              ? float3( 0 )
                              : draw.mEmission.rgb * draw.mEmission.w *
                                        pow( max( PreviewTexture( draw.mTextures0.w, PreviewUv( draw, input, draw.mUv0.w ),
                                                                  float4( 1 ) )
                                                          .rgb,
                                                  0.0 ),
                                             float3( 2.2 ) );
    if ( model == SURFACE_CLEAR_COAT ) {
        emission *= 1 - draw.mSurfaceShading.x * SurfaceCoatFresnel( max( dot( coatNormal, view ), 0.001 ) );
    }
    if ( model == SURFACE_DIELECTRIC && transmission > 0 ) {
        float ior = max( draw.mSurfaceShading.x, 1.0 );
        float3 ray = refract( -view, normal, 1 / ior );
        float pathLength = draw.mSurfaceShading.z / max( abs( dot( ray, normal ) ), 0.1 );
        float3 absorption = pow( max( draw.mDielectricAttenuation.rgb, 0.0001 ), pathLength / max( draw.mDielectricAttenuation.a, 0.001 ) );
        float fresnelV = f0.x + (1 - f0.x) * pow( 1 - nv, 5.0 );
        ambient += PreviewBlurredEnvironment( ray, roughness ) * absorption * base.rgb * transmission * (1 - fresnelV) * scene.mSettings.z;
    }
    float3 color = max( direct + ambient + emission, 0.0 );
    if ( model == SURFACE_UNLIT ) {
        color = base.rgb + emission;
    }
    if ( draw.mUv1.z == 4 ) {
        // Match PostProcessMaterial::Evaluate, using a lit sphere as its input.
        float luminance = dot( color, float3( 0.2126, 0.7152, 0.0722 ) );
        float3 saturated = lerp( float3( luminance ), color, draw.mFactors.w );
        float3 graded = max( ( saturated - 0.5 ) * draw.mFactors.z + 0.5, 0.0 ) * draw.mEmission.rgb;
        color = lerp( color, graded, draw.mEmission.a );
    }
    // The environment pass draws the grid on Y=0. The raised preview ground
    // occludes it; never paint grid lines onto the material's receiving surface.
    return PreviewResult( float4( color, draw.mFlags.x == 2.0 ? saturate( base.a ) : 1.0 ),
                          draw.mUv1.w != 0 ? emission : float3( 0 ) );
}
