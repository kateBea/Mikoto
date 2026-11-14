// PBRT-derived portions: Copyright (c) 1998-2020 Matt Pharr, Wenzel Jakob,
// and Greg Humphreys. SPDX-License-Identifier: Apache-2.0
#pragma once

// TabulatedBSSRDF: sample a projected disk, then reservoir-sample the same-material
// intersections of its probe segment. The axis mixture supplies the area PDF.
// https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/bssrdf.h
// https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/cpu/integrators.cpp
// https://pbr-book.org/3ed-2018/Light_Transport_II_Volume_Rendering/Sampling_Subsurface_Reflection_Functions
static const uint BSSRDF_SAMPLES = 64;

float4 BssrdfTable( Surface s, uint index, bool cdf = false ) {
    return LoadBufferValue<float4>( Parameters.Data, s.Profile + ( index + ( cdf ? BSSRDF_SAMPLES : 0 ) ) * sizeof( float4 ) );
}

float BssrdfRadius( Surface s, uint channel, float sample ) {
    float extinction = s.Absorption[channel] + s.Scattering[channel];
    float total = BssrdfTable( s, BSSRDF_SAMPLES - 1, true )[channel + 1];
    if ( extinction <= 0.0 || total <= 0.0 ) {
        return 0.0;
    }
    float target = sample * total;
    uint low = 0, high = BSSRDF_SAMPLES - 2;
    while ( low < high ) {
        uint mid = ( low + high + 1 ) / 2;
        if ( BssrdfTable( s, mid, true )[channel + 1] <= target ) {
            low = mid;
        }
        else {
            high = mid - 1;
        }
    }

    float4 a = BssrdfTable( s, low ), b = BssrdfTable( s, low + 1 );
    float area = ( target - BssrdfTable( s, low, true )[channel + 1] ) / ( b.x - a.x );
    float start = a[channel + 1], slope = b[channel + 1] - start;
    float denominator = start + sqrt( max( 0.0, start * start + 2.0 * slope * area ) );
    float t = denominator > 0.0 ? 2.0 * area / denominator : 0.0;
    return lerp( a.x, b.x, saturate( t ) ) / extinction;
}

float BssrdfRadial( Surface s, uint channel, float radius, bool pdf ) {
    float extinction = s.Absorption[channel] + s.Scattering[channel];
    float optical = radius * extinction;
    if ( extinction <= 0.0 || optical > BssrdfTable( s, BSSRDF_SAMPLES - 1 ).x ) {
        return 0.0;
    }
    uint low = 0, high = BSSRDF_SAMPLES - 2;
    while ( low < high ) {
        uint mid = ( low + high + 1 ) / 2;
        if ( BssrdfTable( s, mid ).x <= optical ) {
            low = mid;
        }
        else {
            high = mid - 1;
        }
    }
    float4 a = BssrdfTable( s, low ), b = BssrdfTable( s, low + 1 );
    float value = lerp( a[channel + 1], b[channel + 1], ( optical - a.x ) / ( b.x - a.x ) );
    // The first interval has profile(0)=0, so its area density has this finite limit.
    value = optical > 0.0 ? value / ( 2.0 * PI * optical ) : b[channel + 1] / ( 2.0 * PI * b.x );
    if ( pdf ) {
        float total = BssrdfTable( s, BSSRDF_SAMPLES - 1, true )[channel + 1];
        value = total > 0.0 ? value / total : 0.0;
    }
    return value * extinction * extinction;
}

bool SampleBssrdf( Surface entry, inout uint random, out Surface exit, out float3 weight ) {
    exit = ( Surface )0;
    weight = float3( 0 );
    uint channel = min( uint( Random( random ) * 3.0 ), 2u );
    float choose = Random( random );
    float3 tangent = Tangent( entry.Normal ), bitangent = cross( entry.Normal, tangent );
    float3 axis = choose < 0.25 ? tangent : choose < 0.5 ? bitangent : entry.Normal;
    float radius = BssrdfRadius( entry, channel, Random( random ) );
    float maximum = BssrdfRadius( entry, channel, 0.999 );
    if ( maximum <= 0.0 || radius >= maximum ) {
        return false;
    }
    float phi = 2.0 * PI * Random( random );
    float halfLength = sqrt( maximum * maximum - radius * radius );
    float3 start = entry.Position + ToWorld( float3( radius * cos( phi ), radius * sin( phi ), -halfLength ), axis );
    RayDesc ray = { start, 0.0, axis, 2.0 * halfLength };

    // Raw geometry hits preserve outward normals and include both sides of a closed mesh.
    // A reservoir avoids storing a variable-length list of intersected surface points.
    uint count = 0;
    for ( uint crossing = 0; crossing < 256; ++crossing ) {
        Hit hit = ( Hit )0;
        hit.Distance = -1.0;
        TraceRay( LoadRayScene( Parameters.SceneIndex ), RAY_FLAG_NONE, RAY_ALL_INSTANCES, 0, 1, 0, ray, hit );
        if ( hit.Distance < 0.0 ) {
            break;
        }
        Surface candidate = Evaluate( hit.Triangle, hit.Barycentrics );
        bool covered = candidate.AlphaMode == 0 ||
            ( candidate.AlphaMode == 1 ? candidate.Alpha >= candidate.Cutoff : Random( random ) < candidate.Alpha );
        if ( covered && candidate.Material == entry.Material ) {
            ++count;
            if ( Random( random ) * count < 1.0 ) {
                exit = candidate;
                exit.Position = hit.Position;
                exit.PositionError = hit.PositionError;
                exit.GeometricNormal = hit.GeometricNormal;
            }
        }
        ray.TMin = NextFloat( hit.Distance, true );
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

    float3 delta = exit.Position - entry.Position;
    float distance = length( delta );
    float3 local = ToLocal( delta, entry.Normal );
    float3 normal = abs( ToLocal( exit.GeometricNormal, entry.Normal ) );
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
    exit.SubsurfaceExit = true;
    return true;
}
