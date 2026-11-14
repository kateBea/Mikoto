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

// Surface BSDFs and volume phase functions share the transport sampling interface.

// Evaluate and sample the same GGX visible-normal distribution so the BSDF
// and light-sampling PDFs can be compared in solid angle.
float Distribution( float cosine, float alpha ) {
    float cosine2 = cosine * cosine;
    float denominator = max( 1.0 - cosine2, 0.0 ) + cosine2 * alpha * alpha;
    return alpha * alpha / ( PI * denominator * denominator );
}

float Lambda( float cosine, float alpha ) {
    float tangent2 = max( 1.0 - cosine * cosine, 0.0 ) / ( cosine * cosine );
    return ( sqrt( 1.0 + alpha * alpha * tangent2 ) - 1.0 ) * 0.5;
}

float3 Fresnel( float3 f0, float cosine ) {
    // glTF retains Schlick's RGB Fresnel approximation rather than spectral IORs.
    return f0 + ( 1.0 - f0 ) * pow( 1.0 - cosine, 5.0 );
}

float SpecularProbability( Surface s, float3 view, uint lobes ) {
    if ( ( lobes & DIFFUSE ) == 0 || s.Metallic >= 1.0 || !any( s.Base > 0.0 ) ) {
        return 1.0;
    }
    if ( ( lobes & SPECULAR ) == 0 ) {
        return 0.0;
    }

    // The mixture is an engine material adapter; both SampleBSDF and its PDF use it.
    float specular = saturate( MaxComponent( Fresnel( s.F0, saturate( dot( s.Normal, view ) ) ) ) );
    float diffuse = MaxComponent( s.Base ) * ( 1.0 - s.Metallic ) * ( 1.0 - specular );
    return clamp( specular / max( specular + diffuse, 1e-8 ), 0.05, 0.95 );
}

// PBRT's Trowbridge-Reitz VNDF sampler and height-correlated masking-shadowing.
// https://pbr-book.org/4ed/Reflection_Models/Roughness_Using_Microfacet_Theory
// https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/util/scattering.h
float3 SampleVisibleNormal( float3 view, float alpha, float2 sample ) {
    float3 stretched = normalize( float3( alpha * view.xy, view.z ) );
    float3 tangent = stretched.z < 0.99999
                         ? normalize( cross( float3( 0, 0, 1 ), stretched ) )
                         : float3( 1, 0, 0 );
    float3 bitangent = cross( stretched, tangent );

    float radius = sqrt( sample.x );
    float phi = 2.0 * PI * sample.y;
    float2 disk = radius * float2( cos( phi ), sin( phi ) );
    float height = sqrt( max( 0.0, 1.0 - disk.x * disk.x ) );
    disk.y = lerp( height, disk.y, ( 1.0 + stretched.z ) * 0.5 );

    float z = sqrt( max( 0.0, 1.0 - dot( disk, disk ) ) );
    float3 normal = disk.x * tangent + disk.y * bitangent + z * stretched;
    return normalize( float3( alpha * normal.xy, max( normal.z, 1e-6 ) ) );
}

#include "PathTracing_Optics.slang"

float3 EvaluateBSDF( Surface s, float3 view, float3 light, out float pdf, uint lobes = ALL_LOBES ) {
    pdf = 0.0;
    if ( s.Model == MATERIAL_MEDIUM ) {
        pdf = PhaseHG( dot( view, light ), s.Anisotropy );
        return float3( pdf );
    }
    if ( s.SubsurfaceExit ) {
        float cosine = dot( s.Normal, light );
        if ( cosine <= 0.0 || dot( s.GeometricNormal, light ) <= 0.0 ) {
            return float3( 0 );
        }
        pdf = cosine * INV_PI;
        return float3( NormalizedFresnel( s.IOR, cosine ) );
    }
    if ( s.Model == MATERIAL_SUBSURFACE || s.Model == MATERIAL_DIELECTRIC ) {
        return EvaluateDielectric( s, view, light, pdf );
    }
    if ( s.Model == MATERIAL_UNLIT || ( s.Model == MATERIAL_CONDUCTOR && s.Roughness <= 0.01 ) ) {
        return float3( 0 );
    }
    float nv = dot( s.Normal, view );
    float nl = dot( s.Normal, light );
    if ( nv <= 0.0 || nl <= 0.0 || dot( s.GeometricNormal, view ) <= 0.0 ||
         dot( s.GeometricNormal, light ) <= 0.0 || dot( view + light, view + light ) <= 1e-12 ) {
        return float3( 0.0 );
    }

    float3 halfVector = normalize( view + light );
    float nh = saturate( dot( s.Normal, halfVector ) );
    float vh = saturate( dot( view, halfVector ) );
    float alpha = max( s.Roughness * s.Roughness, 0.000625 );
    float distribution = Distribution( nh, alpha );
    float lambdaV = Lambda( nv, alpha );
    float masking = 1.0 / ( 1.0 + lambdaV + Lambda( nl, alpha ) );
    float probability = SpecularProbability( s, view, lobes );

    // Visible-normal density converted to the sampled reflection direction.
    float specularPDF = distribution / ( 4.0 * nv * ( 1.0 + lambdaV ) );
    pdf = probability * specularPDF + ( 1.0 - probability ) * nl * INV_PI;
    float3 fresnel = s.Model == MATERIAL_CONDUCTOR ? ConductorFresnel( vh, s.Absorption, s.Scattering ) * s.Base : Fresnel( s.F0, vh );
    float3 value = float3( 0.0 );
    if ( ( lobes & SPECULAR ) != 0 ) {
        value += fresnel * distribution * masking / ( 4.0 * nv * nl );
    }
    if ( ( lobes & DIFFUSE ) != 0 ) {
        value += ( 1.0 - fresnel ) * ( 1.0 - s.Metallic ) * s.Base * INV_PI;
    }
    return value;
}

BSDFSample SampleBSDF( Surface s, float3 view, inout uint random, uint lobes = ALL_LOBES ) {
    BSDFSample result = ( BSDFSample )0;
    if ( s.Model == MATERIAL_MEDIUM ) {
        float2 sample = Random2D( random );
        result.Direction = SamplePhaseHG( view, s.Anisotropy, sample );
        result.PDF = PhaseHG( dot( view, result.Direction ), s.Anisotropy );
        result.Value = float3( result.PDF );
        return result;
    }
    if ( dot( s.Normal, view ) <= 0.0 || dot( s.GeometricNormal, view ) <= 0.0 ) {
        return result;
    }

    if ( !s.SubsurfaceExit && ( s.Model == MATERIAL_SUBSURFACE || s.Model == MATERIAL_DIELECTRIC ) ) {
        float choose = Random( random );
        float2 sample = Random2D( random );
        return SampleDielectric( s, view, choose, sample );
    }
    if ( s.Model == MATERIAL_CONDUCTOR && s.Roughness <= 0.01 ) {
        result.Direction = reflect( -view, s.Normal );
        result.Value = ConductorFresnel( dot( s.Normal, view ), s.Absorption, s.Scattering ) * s.Base /
                       abs( dot( s.Normal, result.Direction ) );
        result.PDF = 1.0;
        result.Delta = true;
        return result;
    }

    float choose = Random( random );
    float2 sample = Random2D( random );
    if ( !s.SubsurfaceExit && choose < SpecularProbability( s, view, lobes ) ) {
        float3 normal = ToWorld( SampleVisibleNormal( ToLocal( view, s.Normal ), max( s.Roughness * s.Roughness, 0.000625 ), sample ), s.Normal );
        result.Direction = reflect( -view, normal );
    }
    else {
        // Cosine-weighted diffuse sampling, as in PBRT's DiffuseBxDF.
        // https://pbr-book.org/4ed/Reflection_Models/Diffuse_Reflection
        float2 disk = SampleLensDisk( sample );
        result.Direction = ToWorld( float3( disk, sqrt( max( 0.0, 1.0 - dot( disk, disk ) ) ) ), s.Normal );
    }
    result.Value = EvaluateBSDF( s, view, result.Direction, result.PDF, lobes );
    return result;
}
