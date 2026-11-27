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

// Evaluate finite area-light shape, attenuation and representative specular directions.
// Textured emitters sample their source image with the footprint selected for diffuse or
// specular lighting.

#pragma once

#include "LightInfo.slang"
#include "Math_Helpers.slang"

// Raster approximations, evaluated in Mikoto's world space:
// https://alextardif.com/arealights.html
// Brian Karis, Real Shading in Unreal Engine 4, sections Sphere Lights / Tube Lights:
// https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf
// The representative point widens the highlight; normalization compensates its energy
// without changing the material roughness. Rectangle specular follows Tardif's heuristic.
struct AreaLightSample {
    float3 mDiffuseDirection;
    float3 mSpecularDirection;
    float mIrradiance;
    float mSpecularDistance;
    float mSourceRadius;
    float mHalfLength;
    float mRectangleSpecular;
    bool mRectangle;
    float2 mSourceUV;
};

float3 AreaSafeDirection( float3 vector ) {
    return vector * rsqrt( max( dot( vector, vector ), 1e-12 ) );
}

float AreaRangeWindow( float distance, float range ) {
    float window = saturate( 1.0 - pow( distance / max( range, 1e-6 ), 4.0 ) );
    return window * window;
}

// Construct diffuse and representative specular directions for the emitter shape. Source
// radius/length widen the highlight before its energy normalization is applied.
AreaLightSample SampleAreaLight( LightInfo light, float3 position, float3 n, float3 v, float roughness ) {
    AreaLightSample sample = ( AreaLightSample )0;
    RasterAreaShape shape = ( RasterAreaShape )uint( light.mPosition.w );
    float3 center = light.mPosition.xyz - position;
    float3 reflection = reflect( -v, n );
    sample.mDiffuseDirection = AreaSafeDirection( center );
    sample.mSourceRadius = max( light.mPadding, 0.0 );
    float3 closest = center;
    sample.mSourceUV = 0.5;

    if ( shape == RasterAreaShape::eRectangle ) {
        sample.mRectangle = true;
        float3 normal = light.mDirection.xyz;
        float planeDistance = dot( normal, center );
        if ( light.mAreaAxisX.w <= 0 || light.mAreaAxisY.w <= 0 || abs( planeDistance ) < 1e-5 ||
             ( light.mDirection.w < 0.5 && planeDistance >= 0 ) ) {
            return sample;
        }

        float3 right = light.mAreaAxisX.xyz;
        float3 up = light.mAreaAxisY.xyz;
        float2 halfSize = float2( light.mAreaAxisX.w, light.mAreaAxisY.w );
        float3 x = right * halfSize.x;
        float3 y = up * halfSize.y;
        float3 corners[4] = { center - x - y, center + x - y, center + x + y, center - x + y };
        float3 edges[4];
        float cosineSum = saturate( dot( sample.mDiffuseDirection, n ) );

        for ( uint i = 0; i < 4; ++i ) {
            edges[i] = AreaSafeDirection( cross( corners[i], corners[( i + 1 ) % 4] ) );
            cosineSum += saturate( dot( AreaSafeDirection( corners[i] ), n ) );
        }

        float solidAngle = -2.0 * PI_Value;
        for ( uint i = 0; i < 4; ++i ) {
            solidAngle += acos( clamp( dot( -edges[i], edges[( i + 1 ) % 4] ), -1.0, 1.0 ) );
        }

        // Keep the distance cutoff independent of camera direction.
        float2 nearestSurface = clamp( float2( dot( -center, right ), dot( -center, up ) ), -halfSize, halfSize );
        float surfaceDistance = length( center + right * nearestSurface.x + up * nearestSurface.y );
        sample.mIrradiance = max( solidAngle, 0.0 ) * cosineSum * 0.2 * saturate( 1.0 - surfaceDistance / max( light.mRadius, 1e-6 ) );

        float denominator = dot( normal, reflection );
        if ( abs( denominator ) > 1e-6 && planeDistance / denominator > 0 ) {
            float3 intersection = reflection * ( planeDistance / denominator ) - center;
            float2 point = float2( dot( intersection, right ), dot( intersection, up ) );
            float2 nearest = clamp( point, -halfSize, halfSize );
            closest = center + right * nearest.x + up * nearest.y;
            sample.mSourceUV = float2( 0.5 + nearest.x / ( 2 * halfSize.x ), 0.5 - nearest.y / ( 2 * halfSize.y ) );
            float facing = abs( dot( reflection, normal ) );
            sample.mRectangleSpecular = facing * saturate( 1.0 - length( nearest - point ) * pow( 1.0 - roughness, 2.0 ) * 32.0 );
        }
    }
    else if ( shape == RasterAreaShape::eSphere || shape == RasterAreaShape::eTube ) {
        float centerDistance = length( center );
        float rangeDistance = max( centerDistance - sample.mSourceRadius, 0.0 );
        sample.mHalfLength = shape == RasterAreaShape::eTube ? max( light.mAreaAxisX.w, 0.0 ) : 0.0;

        if ( sample.mHalfLength > 1e-6 ) {
            float3 segment = light.mAreaAxisX.xyz * ( 2.0 * sample.mHalfLength );
            float3 start = center - segment * 0.5;
            float3 end = center + segment * 0.5;
            float startDistance = max( length( start ), 1e-6 );
            float endDistance = max( length( end ), 1e-6 );
            float projectedLength = dot( reflection, segment );
            float denominator = dot( segment, segment ) - projectedLength * projectedLength;
            float t = denominator > 1e-8 ? saturate( ( dot( reflection, start ) * projectedLength - dot( start, segment ) ) / denominator ) : saturate( -dot( start, segment ) / max( dot( segment, segment ), 1e-8 ) );
            closest = start + segment * t;

            // UE4 Eq. 16 already integrates inverse-distance falloff along the segment.
            // Apply only the finite-range window afterward; another inverse square would double it.
            float cosine = saturate( dot( n, start ) / ( 2.0 * startDistance ) + dot( n, end ) / ( 2.0 * endDistance ) );
            sample.mIrradiance = 2.0 * cosine / max( startDistance * endDistance + dot( start, end ) + 2.0, 1e-6 );
            float nearestT = saturate( -dot( start, segment ) / max( dot( segment, segment ), 1e-8 ) );
            rangeDistance = max( length( start + nearestT * segment ) - sample.mSourceRadius, 0.0 );
        }
        else {
            // A zero-length tube has the same diffuse response as a sphere.
            sample.mIrradiance = saturate( dot( n, sample.mDiffuseDirection ) ) / ( centerDistance * centerDistance + 1.0 );
        }

        sample.mIrradiance *= AreaRangeWindow( rangeDistance, light.mRadius );
        float3 centerToRay = reflection * max( dot( closest, reflection ), 0.0 ) - closest;
        closest += centerToRay * saturate( sample.mSourceRadius / max( length( centerToRay ), 1e-6 ) );
    }

    sample.mSpecularDistance = max( length( closest ), 1e-4 );
    sample.mSpecularDirection = AreaSafeDirection( closest );
    return sample;
}

// Compensate for the widened specular lobe of the finite emitter. The representative-point
// approximation should not gain energy just because the highlight became broader.
float AreaSpecularNormalization( AreaLightSample sample, float alpha ) {
    alpha = max( alpha, 0.002025 );
    float sphereAlpha = saturate( alpha + sample.mSourceRadius / ( 2.0 * sample.mSpecularDistance ) );
    float normalization = alpha / max( sphereAlpha, 1e-6 );
    normalization *= normalization;
    float lineAlpha = saturate( alpha + sample.mHalfLength / ( 2.0 * sample.mSpecularDistance ) );
    return normalization * alpha / max( lineAlpha, 1e-6 );
}

struct AreaSurfaceLighting {
    float3 mDiffuse;
    float3 mSpecular;
    float3 mDirection;
    float mWeight;
    float2 mSourceUV;
    float mSourceDistance;
};

// Evaluate diffuse and specular response for the representative area-light sample. Visibility
// and light texture modulation are applied by the calling surface shader.
AreaSurfaceLighting EvaluateAreaLight( LightInfo light, float3 position, float3 n, float3 v,
    float3 diffuseColor, float3 f0, float3 f90, float roughness ) {
    AreaSurfaceLighting result = ( AreaSurfaceLighting )0;
    AreaLightSample sample = SampleAreaLight( light, position, n, v, roughness );
    result.mDirection = sample.mSpecularDirection;
    result.mSourceUV = sample.mSourceUV;
    result.mSourceDistance = sample.mSpecularDistance;
    if ( sample.mIrradiance <= 0 ) {
        return result;
    }

    float alpha = max( roughness * roughness, 0.002025 );
    float alphaSquared = alpha * alpha;
    float nv = max( abs( dot( n, v ) ), 0.001 );
    float nl = max( saturate( dot( n, sample.mSpecularDirection ) ), 0.001 );
    float3 h = AreaSafeDirection( v + sample.mSpecularDirection );
    float nh = saturate( dot( n, h ) );
    float vh = saturate( dot( v, h ) );
    float3 fresnel = f0 + ( f90 - f0 ) * pow( 1.0 - vh, 5.0 );
    float distributionDenominator = nh * nh * ( alphaSquared - 1.0 ) + 1.0;
    float distribution = alphaSquared / max( PI_Value * distributionDenominator * distributionDenominator, 1e-10 );
    float geometryV = 2.0 * nv / ( nv + sqrt( alphaSquared + ( 1.0 - alphaSquared ) * nv * nv ) );
    float geometryL = 2.0 * nl / ( nl + sqrt( alphaSquared + ( 1.0 - alphaSquared ) * nl * nl ) );

    float3 diffuseHalf = AreaSafeDirection( v + sample.mDiffuseDirection );
    float diffuseNl = saturate( dot( n, sample.mDiffuseDirection ) );
    float diffuseLh = saturate( dot( sample.mDiffuseDirection, diffuseHalf ) );
    float burley90 = 2.0 * diffuseLh * diffuseLh * alpha - 0.5;
    float3 diffuseFresnel = f0 + ( f90 - f0 ) * pow( 1.0 - saturate( dot( v, diffuseHalf ) ), 5.0 );
    result.mDiffuse = ( 1.0 - diffuseFresnel ) * diffuseColor / PI_Value * sample.mIrradiance *
                     ( 1.0 + burley90 * pow( 1.0 - diffuseNl, 5.0 ) ) * ( 1.0 + burley90 * pow( 1.0 - nv, 5.0 ) );

    result.mSpecular = sample.mRectangle ? fresnel * sample.mRectangleSpecular * sample.mIrradiance : fresnel * geometryV * geometryL * distribution * AreaSpecularNormalization( sample, alpha ) * sample.mIrradiance / ( 4.0 * nl * nv );
    result.mWeight = sample.mIrradiance / nl;
    return result;
}

// Texturing extends the representative-point rectangle approximation above.
// Diffuse uses a normalized quadrature of source radiance; specular uses the
// reflection-plane point with a roughness footprint. This is not an LTC integral.
// https://alextardif.com/arealights.html (rectangle plane/nearest-point mapping)
float3 AreaTextureColor( LightInfo light, Texture2D source, SamplerState sourceSampler, float2 uv, float mip ) {
    uv = saturate( uv ) * light.mAreaEmissionUv.xy + light.mAreaEmissionUv.zw;
    float3 color = max( source.SampleLevel( sourceSampler, uv, mip ).rgb, 0 );
    // Video/HDR resources and sRGB views already sample in linear space.
    if ( light.mAreaEmission.z != 0 ) {
        for ( uint channel = 0; channel < 3; ++channel ) {
            color[channel] = color[channel] <= 0.04045 ? color[channel] / 12.92 : pow( ( color[channel] + 0.055 ) / 1.055, 2.4 );
        }
    }
    return color;
}

// Filter source-emission texture for the diffuse footprint. A wide footprint averages emitter
// detail rather than projecting sharp texels onto every receiver.
float3 AreaDiffuseTexture( LightInfo light, float3 position, float3 normal, Texture2D source, SamplerState sourceSampler ) {
    uint width;
    uint height;
    source.GetDimensions( width, height );
    float textureSpan = max( width * abs( light.mAreaEmissionUv.x ), height * abs( light.mAreaEmissionUv.y ) );
    float mip = clamp( log2( max( textureSpan / 3, 1 ) ), 0, float( max( light.mAreaEmission.w, 1u ) - 1 ) );
    float3 total = 0;
    float weights = 0;

    // Integrate colour across the emitter instead of projecting a single
    // sharp texel onto diffuse receivers. Near receivers favour nearby cells.
    for ( uint y = 0; y < 3; ++y ) {
        for ( uint x = 0; x < 3; ++x ) {
            float2 uv = ( float2( x, y ) + 0.5 ) / 3;
            float3 delta = light.mPosition.xyz - position + light.mAreaAxisX.xyz * ( ( uv.x - 0.5 ) * 2 * light.mAreaAxisX.w ) +
                           light.mAreaAxisY.xyz * ( ( 0.5 - uv.y ) * 2 * light.mAreaAxisY.w );
            float distanceSquared = max( dot( delta, delta ), 1e-8 );
            float3 incoming = delta * rsqrt( distanceSquared );
            float facing = dot( light.mDirection.xyz, -incoming );
            facing = light.mDirection.w > 0.5 ? abs( facing ) : saturate( facing );
            float weight = saturate( dot( normal, incoming ) ) * facing / distanceSquared;
            total += AreaTextureColor( light, source, sourceSampler, uv, mip ) * weight;
            weights += weight;
        }
    }
    return weights > 1e-8 ? total / weights : float3( 0 );
}

// Sample source emission at the representative specular point with a roughness-dependent
// footprint. Broader reflections use a more filtered emitter image.
float3 AreaSpecularTexture( LightInfo light, AreaSurfaceLighting area, float roughness, Texture2D source, SamplerState sourceSampler ) {
    uint width;
    uint height;
    source.GetDimensions( width, height );
    float2 radius = min( roughness * roughness * area.mSourceDistance /
                             max( 2 * float2( light.mAreaAxisX.w, light.mAreaAxisY.w ), 1e-5 ),
        1 );
    float texelRadius = max( radius.x * width * abs( light.mAreaEmissionUv.x ), radius.y * height * abs( light.mAreaEmissionUv.y ) );
    if ( light.mAreaEmission.w > 1 || texelRadius <= 1 ) {
        float mip = clamp( log2( max( texelRadius, 1 ) ), 0, float( max( light.mAreaEmission.w, 1u ) - 1 ) );
        return AreaTextureColor( light, source, sourceSampler, area.mSourceUV, mip );
    }

    // Decoder outputs have one mip. Filter them explicitly so changing video
    // does not turn rough highlights into sharp, shimmering copies of the frame.
    float3 total = 0;
    for ( int y = -1; y <= 1; ++y ) {
        for ( int x = -1; x <= 1; ++x ) {
            total += AreaTextureColor( light, source, sourceSampler, area.mSourceUV + float2( x, y ) * radius, 0 );
        }
    }
    return total / 9;
}
