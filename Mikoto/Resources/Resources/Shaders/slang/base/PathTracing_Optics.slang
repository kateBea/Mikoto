// PBRT-derived portions: Copyright (c) 1998-2020 Matt Pharr, Wenzel Jakob,
// and Greg Humphreys. SPDX-License-Identifier: Apache-2.0
#pragma once

// Exact dielectric Fresnel, Snell refraction and rough transmission Jacobians.
// https://pbr-book.org/4ed/Reflection_Models/Specular_Reflection_and_Transmission
// https://pbr-book.org/4ed/Reflection_Models/Dielectric_BSDF
// https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/bxdfs.cpp
float DielectricFresnel( float cosine, float eta ) {
    cosine = clamp( cosine, -1.0, 1.0 );
    if ( cosine < 0 ) {
        eta = 1.0 / eta;
        cosine = -cosine;
    }
    float sine2 = max( 0.0, 1.0 - cosine * cosine ) / ( eta * eta );
    if ( sine2 >= 1.0 ) {
        return 1.0;
    }
    float transmitted = sqrt( 1.0 - sine2 );
    float parallel = ( eta * cosine - transmitted ) / ( eta * cosine + transmitted );
    float perpendicular = ( cosine - eta * transmitted ) / ( cosine + eta * transmitted );
    return 0.5 * ( parallel * parallel + perpendicular * perpendicular );
}

// Complex-IOR Fresnel, evaluated per RGB channel. This is not a spectral conversion.
// https://pbr-book.org/4ed/Reflection_Models/Conductor_BRDF
// https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/util/scattering.h
float3 ConductorFresnel( float cosine, float3 eta, float3 k ) {
    cosine = saturate( abs( cosine ) );
    float cosine2 = cosine * cosine, sine2 = 1.0 - cosine2;
    float3 eta2 = eta * eta, k2 = k * k;
    float3 t0 = eta2 - k2 - sine2;
    float3 a2b2 = sqrt( t0 * t0 + 4.0 * eta2 * k2 );
    float3 a = sqrt( max( float3( 0 ), 0.5 * ( a2b2 + t0 ) ) );
    float3 t1 = a2b2 + cosine2, t2 = 2.0 * cosine * a;
    float3 rs = ( t1 - t2 ) / max( t1 + t2, float3( 1e-20 ) );
    float3 t3 = cosine2 * a2b2 + sine2 * sine2, t4 = t2 * sine2;
    float3 rp = rs * ( t3 - t4 ) / max( t3 + t4, float3( 1e-20 ) );
    return 0.5 * ( rs + rp );
}

bool DielectricRefract( float3 view, float3 normal, float eta, out float3 direction ) {
    float cosine = dot( normal, view );
    float sine2 = max( 0.0, 1.0 - cosine * cosine ) / ( eta * eta );
    direction = float3( 0 );
    if ( sine2 >= 1.0 ) {
        return false;
    }
    direction = -view / eta + ( cosine / eta - sqrt( 1.0 - sine2 ) ) * normal;
    return true;
}

float NormalizedFresnel( float eta, float cosine ) {
    // PBRT NormalizedFresnelBxDF, including its radiance-mode eta^2 factor.
    // https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/bxdfs.h
    float inverse = 1.0 / eta;
    float moment = 0.45966 + inverse * ( -1.73965 + inverse * ( 3.37668 +
        inverse * ( -3.904945 + inverse * ( 2.49277 - 0.68441 * inverse ) ) ) );
    return ( 1.0 - DielectricFresnel( cosine, eta ) ) * eta * eta / ( PI * ( 1.0 - 2.0 * moment ) );
}

float3 EvaluateDielectric( Surface s, float3 view, float3 light, out float pdf ) {
    pdf = 0.0;
    if ( s.ThinWalled || s.Roughness <= 0.01 || s.IOR == 1.0 ) {
        return float3( 0 );
    }

    float nv = dot( s.Normal, view ), nl = dot( s.Normal, light );
    // Reject paths that go through the wrong side of the geometric surface.
    bool reflection = nl > 0.0;
    if ( nv <= 0.0 || nl == 0.0 || ( dot( s.GeometricNormal, light ) > 0.0 ) != reflection ) {
        return float3( 0 );
    }
    float eta = s.FrontFace ? s.IOR : 1.0 / s.IOR;
    // Reflection and transmission use different microfacet half-vectors.
    float3 halfVector = view + light * ( reflection ? 1.0 : eta );
    if ( dot( halfVector, halfVector ) < 1e-20 ) {
        return float3( 0 );
    }
    halfVector = normalize( halfVector );
    if ( dot( halfVector, s.Normal ) < 0.0 ) {
        halfVector = -halfVector;
    }
    float vh = dot( view, halfVector ), lh = dot( light, halfVector );
    if ( vh <= 0.0 || ( reflection ? lh <= 0.0 : lh >= 0.0 ) ) {
        return float3( 0 );
    }

    float alpha = max( s.Roughness * s.Roughness, 1e-6 );
    float distribution = Distribution( dot( s.Normal, halfVector ), alpha );
    float g1 = 1.0 / ( 1.0 + Lambda( nv, alpha ) );
    float masking = 1.0 / ( 1.0 + Lambda( nv, alpha ) + Lambda( abs( nl ), alpha ) );
    float fresnel = DielectricFresnel( vh, eta );
    float reflected = fresnel, transmitted = ( 1.0 - fresnel ) * s.Transmission;
    float sum = reflected + transmitted;
    if ( sum <= 0.0 ) {
        return float3( 0 );
    }
    float normalPDF = distribution * g1 * vh / nv;
    if ( reflection ) {
        pdf = normalPDF * ( reflected / sum ) / ( 4.0 * vh );
        return float3( fresnel * distribution * masking / ( 4.0 * nv * abs( nl ) ) );
    }

    float denominator = lh + vh / eta;
    denominator *= denominator;
    if ( denominator <= 1e-20 ) {
        return float3( 0 );
    }
    pdf = normalPDF * ( transmitted / sum ) * abs( lh ) / denominator;
    float value = transmitted * distribution * masking * abs( lh * vh / ( nl * nv * denominator ) );
    return float3( value / ( eta * eta ) );
}

BSDFSample SampleDielectric( Surface s, float3 view, float choose, float2 sample ) {
    BSDFSample result = ( BSDFSample )0;
    float eta = s.FrontFace || s.ThinWalled ? s.IOR : 1.0 / s.IOR;
    bool smooth = s.ThinWalled || s.Roughness <= 0.01 || s.IOR == 1.0;
    float3 normal = smooth ? s.Normal : ToWorld( SampleVisibleNormal(
        ToLocal( view, s.Normal ), s.Roughness * s.Roughness, sample ), s.Normal );
    float cosine = dot( normal, view );
    if ( cosine <= 0.0 ) {
        return result;
    }
    float reflected = DielectricFresnel( cosine, eta );
    if ( s.ThinWalled ) {
        // PBRT ThinDielectricBxDF sums the internal reflections of a thin sheet.
        reflected = 2.0 * reflected / ( 1.0 + reflected );
    }
    float transmitted = ( 1.0 - reflected ) * s.Transmission;
    float sum = reflected + transmitted;
    if ( sum <= 0.0 ) {
        return result;
    }

    result.Transmitted = choose >= reflected / sum;
    if ( result.Transmitted ) {
        if ( s.ThinWalled ) {
            result.Direction = -view;
        }
        else if ( !DielectricRefract( view, normal, eta, result.Direction ) ) {
            return ( BSDFSample )0;
        }
    }
    else {
        result.Direction = reflect( -view, normal );
    }

    if ( ( dot( s.GeometricNormal, result.Direction ) < 0.0 ) != result.Transmitted ) {
        return ( BSDFSample )0;
    }
    result.Eta = result.Transmitted && !s.ThinWalled ? eta : 1.0;
    result.Delta = smooth;
    if ( smooth ) {
        float value = result.Transmitted ? transmitted : reflected;
        result.PDF = value / sum;
        value /= abs( dot( s.Normal, result.Direction ) );
        if ( result.Transmitted ) {
            value /= result.Eta * result.Eta;
        }
        result.Value = float3( value );
    }
    else {
        result.Value = EvaluateDielectric( s, view, result.Direction, result.PDF );
    }
    return result;
}
