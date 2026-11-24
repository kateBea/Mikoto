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

#include "LightInfo.slang"
#include "Math_Helpers.slang"

// Raster approximations, evaluated in Mikoto's world space:
// https://alextardif.com/arealights.html
// Brian Karis, Real Shading in Unreal Engine 4, sections Sphere Lights / Tube Lights:
// https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf
// The representative point widens the highlight; normalization compensates its energy
// without changing the material roughness. Rectangle specular follows Tardif's heuristic.
struct AreaLightSample {
    float3 DiffuseDirection;
    float3 SpecularDirection;
    float Irradiance;
    float SpecularDistance;
    float SourceRadius;
    float HalfLength;
    float RectangleSpecular;
    bool Rectangle;
};

float3 AreaSafeDirection( float3 vector ) {
    return vector * rsqrt( max( dot( vector, vector ), 1e-12 ) );
}

float AreaRangeWindow( float distance, float range ) {
    float window = saturate( 1.0 - pow( distance / max( range, 1e-6 ), 4.0 ) );
    return window * window;
}

AreaLightSample SampleAreaLight( LightInfo light, float3 position, float3 n, float3 v, float roughness ) {
    AreaLightSample sample = (AreaLightSample)0;
    RasterAreaShape shape = (RasterAreaShape)uint( light.Position.w );
    float3 center = light.Position.xyz - position;
    float3 reflection = reflect( -v, n );
    sample.DiffuseDirection = AreaSafeDirection( center );
    sample.SourceRadius = max( light.Padding, 0.0 );
    float3 closest = center;

    if ( shape == RasterAreaShape::eRectangle ) {
        sample.Rectangle = true;
        float3 normal = light.Direction.xyz;
        float planeDistance = dot( normal, center );
        if ( light.AreaAxisX.w <= 0 || light.AreaAxisY.w <= 0 || abs( planeDistance ) < 1e-5 ||
             ( light.Direction.w < 0.5 && planeDistance >= 0 ) ) {
            return sample;
        }

        float3 right = light.AreaAxisX.xyz;
        float3 up = light.AreaAxisY.xyz;
        float2 halfSize = float2( light.AreaAxisX.w, light.AreaAxisY.w );
        float3 x = right * halfSize.x;
        float3 y = up * halfSize.y;
        float3 corners[4] = { center - x - y, center + x - y, center + x + y, center - x + y };
        float3 edges[4];
        float cosineSum = saturate( dot( sample.DiffuseDirection, n ) );

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
        sample.Irradiance = max( solidAngle, 0.0 ) * cosineSum * 0.2 * saturate( 1.0 - surfaceDistance / max( light.Radius, 1e-6 ) );

        float denominator = dot( normal, reflection );
        if ( abs( denominator ) > 1e-6 && planeDistance / denominator > 0 ) {
            float3 intersection = reflection * ( planeDistance / denominator ) - center;
            float2 point = float2( dot( intersection, right ), dot( intersection, up ) );
            float2 nearest = clamp( point, -halfSize, halfSize );
            closest = center + right * nearest.x + up * nearest.y;
            float facing = abs( dot( reflection, normal ) );
            sample.RectangleSpecular = facing * saturate( 1.0 - length( nearest - point ) * pow( 1.0 - roughness, 2.0 ) * 32.0 );
        }
    }
    else if ( shape == RasterAreaShape::eSphere || shape == RasterAreaShape::eTube ) {
        float centerDistance = length( center );
        float rangeDistance = max( centerDistance - sample.SourceRadius, 0.0 );
        sample.HalfLength = shape == RasterAreaShape::eTube ? max( light.AreaAxisX.w, 0.0 ) : 0.0;

        if ( sample.HalfLength > 1e-6 ) {
            float3 segment = light.AreaAxisX.xyz * ( 2.0 * sample.HalfLength );
            float3 start = center - segment * 0.5;
            float3 end = center + segment * 0.5;
            float startDistance = max( length( start ), 1e-6 );
            float endDistance = max( length( end ), 1e-6 );
            float projectedLength = dot( reflection, segment );
            float denominator = dot( segment, segment ) - projectedLength * projectedLength;
            float t = denominator > 1e-8 ?
                saturate( ( dot( reflection, start ) * projectedLength - dot( start, segment ) ) / denominator ) :
                saturate( -dot( start, segment ) / max( dot( segment, segment ), 1e-8 ) );
            closest = start + segment * t;

            // UE4 Eq. 16 already integrates inverse-distance falloff along the segment.
            // Apply only the finite-range window afterward; another inverse square would double it.
            float cosine = saturate( dot( n, start ) / ( 2.0 * startDistance ) + dot( n, end ) / ( 2.0 * endDistance ) );
            sample.Irradiance = 2.0 * cosine / max( startDistance * endDistance + dot( start, end ) + 2.0, 1e-6 );
            float nearestT = saturate( -dot( start, segment ) / max( dot( segment, segment ), 1e-8 ) );
            rangeDistance = max( length( start + nearestT * segment ) - sample.SourceRadius, 0.0 );
        }
        else {
            // A zero-length tube has the same diffuse response as a sphere.
            sample.Irradiance = saturate( dot( n, sample.DiffuseDirection ) ) / ( centerDistance * centerDistance + 1.0 );
        }

        sample.Irradiance *= AreaRangeWindow( rangeDistance, light.Radius );
        float3 centerToRay = reflection * max( dot( closest, reflection ), 0.0 ) - closest;
        closest += centerToRay * saturate( sample.SourceRadius / max( length( centerToRay ), 1e-6 ) );
    }

    sample.SpecularDistance = max( length( closest ), 1e-4 );
    sample.SpecularDirection = AreaSafeDirection( closest );
    return sample;
}

float AreaSpecularNormalization( AreaLightSample sample, float alpha ) {
    alpha = max( alpha, 0.002025 );
    float sphereAlpha = saturate( alpha + sample.SourceRadius / ( 2.0 * sample.SpecularDistance ) );
    float normalization = alpha / max( sphereAlpha, 1e-6 );
    normalization *= normalization;
    float lineAlpha = saturate( alpha + sample.HalfLength / ( 2.0 * sample.SpecularDistance ) );
    return normalization * alpha / max( lineAlpha, 1e-6 );
}

struct AreaSurfaceLighting {
    float3 Diffuse;
    float3 Specular;
    float3 Direction;
    float Weight;
};

AreaSurfaceLighting EvaluateAreaLight( LightInfo light, float3 position, float3 n, float3 v,
    float3 diffuseColor, float3 f0, float3 f90, float roughness ) {
    AreaSurfaceLighting result = (AreaSurfaceLighting)0;
    AreaLightSample sample = SampleAreaLight( light, position, n, v, roughness );
    result.Direction = sample.SpecularDirection;
    if ( sample.Irradiance <= 0 ) {
        return result;
    }

    float alpha = max( roughness * roughness, 0.002025 );
    float alphaSquared = alpha * alpha;
    float nv = max( abs( dot( n, v ) ), 0.001 );
    float nl = max( saturate( dot( n, sample.SpecularDirection ) ), 0.001 );
    float3 h = AreaSafeDirection( v + sample.SpecularDirection );
    float nh = saturate( dot( n, h ) );
    float vh = saturate( dot( v, h ) );
    float3 fresnel = f0 + ( f90 - f0 ) * pow( 1.0 - vh, 5.0 );
    float distributionDenominator = nh * nh * ( alphaSquared - 1.0 ) + 1.0;
    float distribution = alphaSquared / max( PI_Value * distributionDenominator * distributionDenominator, 1e-10 );
    float geometryV = 2.0 * nv / ( nv + sqrt( alphaSquared + ( 1.0 - alphaSquared ) * nv * nv ) );
    float geometryL = 2.0 * nl / ( nl + sqrt( alphaSquared + ( 1.0 - alphaSquared ) * nl * nl ) );

    float3 diffuseHalf = AreaSafeDirection( v + sample.DiffuseDirection );
    float diffuseNl = saturate( dot( n, sample.DiffuseDirection ) );
    float diffuseLh = saturate( dot( sample.DiffuseDirection, diffuseHalf ) );
    float burley90 = 2.0 * diffuseLh * diffuseLh * alpha - 0.5;
    float3 diffuseFresnel = f0 + ( f90 - f0 ) * pow( 1.0 - saturate( dot( v, diffuseHalf ) ), 5.0 );
    result.Diffuse = ( 1.0 - diffuseFresnel ) * diffuseColor / PI_Value * sample.Irradiance *
        ( 1.0 + burley90 * pow( 1.0 - diffuseNl, 5.0 ) ) * ( 1.0 + burley90 * pow( 1.0 - nv, 5.0 ) );

    result.Specular = sample.Rectangle ? fresnel * sample.RectangleSpecular * sample.Irradiance :
        fresnel * geometryV * geometryL * distribution * AreaSpecularNormalization( sample, alpha ) * sample.Irradiance / ( 4.0 * nl * nv );
    result.Weight = sample.Irradiance / nl;
    return result;
}
