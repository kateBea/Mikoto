// PBRT-derived portions: Copyright (c) 1998-2020 Matt Pharr, Wenzel Jakob,
// and Greg Humphreys. SPDX-License-Identifier: Apache-2.0

// Sample subsurface transport using the uploaded diffusion profile. Surface probes search for
// an exit point and return the probability/weight for the selected radial sample.

#pragma once

// TabulatedBSSRDF: sample a projected disk, then reservoir-sample the same-material
// intersections of its probe segment. The axis mixture supplies the area PDF.
// https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/bssrdf.h
// https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/cpu/integrators.cpp
// https://pbr-book.org/3ed-2018/Light_Transport_II_Volume_Rendering/Sampling_Subsurface_Reflection_Functions
static const uint BSSRDF_SAMPLES = 64;

float4 BssrdfTable( Surface s, uint index, bool cdf = false ) {
    return LoadBufferValue<float4>( Parameters.mData, s.mProfile + ( index + ( cdf ? BSSRDF_SAMPLES : 0 ) ) * sizeof( float4 ) );
}

// Sample a diffusion-profile radius from the uploaded table. The returned radius determines
// where to probe for a possible subsurface exit.
float BssrdfRadius( Surface s, uint channel, float sample ) {
    float extinction = s.mAbsorption[channel] + s.mScattering[channel];
    float total = BssrdfTable( s, BSSRDF_SAMPLES - 1, true )[channel + 1];
    if ( extinction <= 0.0 || total <= 0.0 ) {
        return 0.0;
    }
    float target = sample * total;
    uint low = 0;
    uint high = BSSRDF_SAMPLES - 2;
    while ( low < high ) {
        uint mid = ( low + high + 1 ) / 2;
        if ( BssrdfTable( s, mid, true )[channel + 1] <= target ) {
            low = mid;
        }
        else {
            high = mid - 1;
        }
    }

    float4 a = BssrdfTable( s, low );
    float4 b = BssrdfTable( s, low + 1 );
    float area = ( target - BssrdfTable( s, low, true )[channel + 1] ) / ( b.x - a.x );
    float start = a[channel + 1];
    float slope = b[channel + 1] - start;
    float denominator = start + sqrt( max( 0.0, start * start + 2.0 * slope * area ) );
    float t = denominator > 0.0 ? 2.0 * area / denominator : 0.0;
    return lerp( a.x, b.x, saturate( t ) ) / extinction;
}

float BssrdfRadial( Surface s, uint channel, float radius, bool pdf ) {
    float extinction = s.mAbsorption[channel] + s.mScattering[channel];
    float optical = radius * extinction;
    if ( extinction <= 0.0 || optical > BssrdfTable( s, BSSRDF_SAMPLES - 1 ).x ) {
        return 0.0;
    }
    uint low = 0;
    uint high = BSSRDF_SAMPLES - 2;
    while ( low < high ) {
        uint mid = ( low + high + 1 ) / 2;
        if ( BssrdfTable( s, mid ).x <= optical ) {
            low = mid;
        }
        else {
            high = mid - 1;
        }
    }
    float4 a = BssrdfTable( s, low );
    float4 b = BssrdfTable( s, low + 1 );
    float value = lerp( a[channel + 1], b[channel + 1], ( optical - a.x ) / ( b.x - a.x ) );
    // The first interval has profile(0)=0, so its area density has this finite limit.
    value = optical > 0.0 ? value / ( 2.0 * PI * optical ) : b[channel + 1] / ( 2.0 * PI * b.x );
    if ( pdf ) {
        float total = BssrdfTable( s, BSSRDF_SAMPLES - 1, true )[channel + 1];
        value = total > 0.0 ? value / total : 0.0;
    }
    return value * extinction * extinction;
}

// Probe the object for an exit point and weight the chosen surface sample by the diffusion
// profile and selection PDF. The integrator resumes surface transport from that exit.
// Bound the number of candidate surface crossings visited by one subsurface exit probe.
static const uint kBssrdfProbeCrossingLimit = 256;

bool SampleBssrdf( Surface entry, inout uint random, out Surface exit, out float3 weight ) {
    exit = ( Surface )0;
    weight = float3( 0 );
    uint channel = min( uint( Random( random ) * 3.0 ), 2u );
    float choose = Random( random );
    float3 tangent = Tangent( entry.mNormal );
    float3 bitangent = cross( entry.mNormal, tangent );
    float3 axis = choose < 0.25 ? tangent : choose < 0.5 ? bitangent : entry.mNormal;
    float radius = BssrdfRadius( entry, channel, Random( random ) );
    float maximum = BssrdfRadius( entry, channel, 0.999 );
    if ( maximum <= 0.0 || radius >= maximum ) {
        return false;
    }
    float phi = 2.0 * PI * Random( random );
    float halfLength = sqrt( maximum * maximum - radius * radius );
    float3 start = entry.mPosition + ToWorld( float3( radius * cos( phi ), radius * sin( phi ), -halfLength ), axis );
    RayDesc ray = { start, 0.0, axis, 2.0 * halfLength };

    // Raw geometry hits preserve outward normals and include both sides of a closed mesh.
    // A reservoir avoids storing a variable-length list of intersected surface points.
    uint count = 0;
    for ( uint crossing = 0; crossing < kBssrdfProbeCrossingLimit; ++crossing ) {
        Hit hit = ( Hit )0;
        hit.mDistance = -1.0;
        TraceRay( LoadRayScene( Parameters.mSceneIndex ), RAY_FLAG_NONE, RAY_ALL_INSTANCES, 0, 1, 0, ray, hit );
        if ( hit.mDistance < 0.0 ) {
            break;
        }
        Surface candidate = Evaluate( hit.mTriangle, hit.mBarycentrics );
        bool covered = candidate.mAlphaMode == 0 ||
            ( candidate.mAlphaMode == 1 ? candidate.mAlpha >= candidate.mCutoff : Random( random ) < candidate.mAlpha );
        if ( covered && candidate.mMaterial == entry.mMaterial ) {
            ++count;
            if ( Random( random ) * count < 1.0 ) {
                exit = candidate;
                exit.mPosition = hit.mPosition;
                exit.mPositionError = hit.mPositionError;
                exit.mGeometricNormal = hit.mGeometricNormal;
            }
        }
        ray.TMin = NextFloat( hit.mDistance, true );
        if ( ray.TMin >= ray.TMax ) {
            break;
        }
        if ( crossing == 255 ) {
            return false;
        }
    }
    if ( count == 0 ) {
        return false;
    }

    float3 delta = exit.mPosition - entry.mPosition;
    float distance = length( delta );
    float3 local = ToLocal( delta, entry.mNormal );
    float3 normal = abs( ToLocal( exit.mGeometricNormal, entry.mNormal ) );
    float3 projected = sqrt( max( float3( 0 ), dot( local, local ) - local * local ) );
    float pdf = 0.0;
    for ( uint c = 0; c < 3; ++c ) {
        if ( distance <= BssrdfRadius( entry, c, 0.999 ) ) {
            pdf += ( 0.25 * normal.x * BssrdfRadial( entry, c, projected.x, true ) +
                     0.25 * normal.y * BssrdfRadial( entry, c, projected.y, true ) +
                     0.5 * normal.z * BssrdfRadial( entry, c, projected.z, true ) ) / 3.0;
        }
        weight[c] = BssrdfRadial( entry, c, distance, false );
    }
    if ( pdf <= 0.0 ) {
        return false;
    }
    weight *= float( count ) / pdf;
    exit.mSubsurfaceExit = true;
    return true;
}
