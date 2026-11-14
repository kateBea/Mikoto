// Copyright 2026 ケイト. Licensed under the Apache License, Version 2.0.
// PBRT-derived portions: Copyright (c) 1998-2016 Matt Pharr, Greg Humphreys,
// and Wenzel Jakob. The PBRT v3 redistribution notice is in Renderer/PBRT-NOTICE.md.
#pragma once

#include "Volume_Helpers.slang"

// Homogeneous-medium distance sampling and its RGB channel-mixture density follow PBRT.
// https://pbr-book.org/3ed-2018/Volume_Scattering/Media
// https://github.com/mmp/pbrt-v3/blob/master/src/media/homogeneous.cpp
// Henyey-Greenstein convention/sampling follows PBRT v4 (both directions point away).
// https://pbr-book.org/4ed/Volume_Scattering/Phase_Functions
// https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/util/sampling.cpp

MediumState ReadMedium( MediumStack media, uint depth ) {
    MediumState result = ( MediumState )0;
    result.IOR = 1.0;
    if ( depth > 0 ) {
        MaterialRecord material = LoadBufferValue<MaterialRecord>( Parameters.Data, 32 + media.Materials[depth - 1] * sizeof(MaterialRecord) );
        result.Absorption = material.Absorption.xyz;
        result.Scattering = material.Scattering.xyz;
        result.Anisotropy = material.Absorption.w;
        result.IOR = material.Optical.x;
    }
    return result;
}

bool CrossMediumBoundary( inout MediumStack media, Surface boundary ) {
    if ( boundary.FrontFace ) {
        if ( media.Count == 8 ) {
            return false;
        }
        media.Materials[media.Count++] = boundary.Material;
    }
    else if ( media.Count > 0 ) {
        --media.Count;
    }
    return true;
}

bool NullMediumBoundary( Surface surface, MediumStack media ) {
    uint outside = surface.FrontFace ? media.Count : ( media.Count > 1 ? media.Count - 1 : 0 );
    return surface.Model == MATERIAL_DIELECTRIC && !surface.ThinWalled &&
           surface.IOR == ReadMedium( media, outside ).IOR && surface.Transmission == 1.0;
}

float PhaseHG( float cosine, float g ) {
    float denominator = 1.0 + g * g + 2.0 * g * cosine;
    return INV_FOUR_PI * ( 1.0 - g * g ) / ( denominator * sqrt( denominator ) );
}

float3 SamplePhaseHG( float3 view, float g, float2 sample ) {
    // Algebraic form of PBRT's inverse CDF, stable as g approaches zero.
    float k = 1.0 - 2.0 * sample.x;
    float denominator = 1.0 + g * k;
    float cosine = -( k + 0.5 * g * ( 3.0 + k * k ) + g * g * k +
                      0.5 * g * g * g * ( k * k - 1.0 ) ) / ( denominator * denominator );
    return Hemisphere( view, clamp( cosine, -1.0, 1.0 ), 2.0 * PI * sample.y );
}

bool SampleMedium( MediumState medium, float maximum, float2 sample, out float distance, out float3 weight ) {
    distance = maximum;
    if ( !any( medium.Scattering > 0.0 ) ) {
        // Pure absorption has an exact deterministic solution and needs no random walk.
        weight = exp( -medium.Absorption * maximum );
        return false;
    }
    float3 extinction = medium.Absorption + medium.Scattering;
    uint channel = min( uint( sample.x * 3.0 ), 2u );
    float freeFlight = extinction[channel] > 0.0 ? -log( 1.0 - sample.y ) / extinction[channel] : 1e30;
    bool scatter = freeFlight < maximum;
    distance = min( freeFlight, maximum );
    float3 transmittance = exp( -extinction * distance );
    float3 density = scatter ? extinction * transmittance : transmittance;
    float pdf = ( density.x + density.y + density.z ) / 3.0;
    weight = pdf > 0.0 ? transmittance * ( scatter ? medium.Scattering : float3( 1 ) ) / pdf : float3( 0 );
    return scatter;
}

// Sample each overlapping constituent independently and keep the closest free flight.
// This is the component-composition rule from section 4 of Kutz et al. (2017), not
// an implementation of their spectral/weighted tracking estimator.
// https://jannovak.info/publications/SDTracking/SDTracking.pdf
// PBRT supplies the absorption/scattering events and heterogeneous delta tracking;
// Mikoto retains one compensated RGB channel per path instead of spectral wavelengths.
// https://pbr-book.org/4ed/Light_Transport_II_Volume_Rendering/Volume_Scattering_Integrators
bool SampleSceneFog( float3 origin, float3 direction, float maximum, inout uint random,
    inout int channel, out float distance, out float3 weight, out uint mediumIndex ) {
    distance = maximum;
    weight = 1.0;
    mediumIndex = 0xffffffffu;
    uint count = VolumeCount( Parameters.Volumes );

    for ( uint index = 0; index < count; ++index ) {
        VolumeRecord medium = LoadVolume( Parameters.Volumes, index );
        float entry, exit;

        if ( !any( medium.ExtinctionMajorant.xyz > 0.0 ) ||
             !VolumeRayInterval( medium, origin, direction, entry, exit ) ) {
            continue;
        }

        entry = max( entry, 0.0 );
        exit = min( exit, distance );

        if ( exit <= entry ) {
            continue;
        }

        if ( channel < 0 ) {
            channel = min( int( Random( random ) * 3.0 ), 2 );
            weight = 0.0;
            weight[channel] = 3.0;
        }

        float extinction = medium.ExtinctionMajorant[channel];

        if ( extinction <= 0.0 ) {
            continue;
        }

        if ( medium.Flags.y <= 1 ) {
            VolumeAnalyticProfile profile = MakeVolumeAnalyticProfile( medium, origin, direction, entry, exit );
            float integral = -log( max( 1.0 - Random( random ), 1e-7 ) ) / extinction;
            float sampleDistance;

            if ( SampleVolumeAnalytic( profile, integral, sampleDistance ) ) {
                distance = entry + sampleDistance;
                mediumIndex = index;
            }

            continue;
        }

        // Ray bounds and record are cached for this medium, including every null event.
        float majorant = extinction * 1.00001;
        float coefficient = medium.AbsorptionDensity[channel] + medium.ScatteringPhase[channel];
        float t = entry;

        while ( true ) {
            t += -log( max( 1.0 - Random( random ), 1e-7 ) ) / majorant;

            if ( t >= exit ) {
                break;
            }

            float density = VolumeDensity( Parameters.Volumes, medium, origin + direction * t, false );

            if ( Random( random ) * majorant < coefficient * density ) {
                distance = t;
                mediumIndex = index;
                break;
            }
        }
    }

    if ( mediumIndex == 0xffffffffu ) {
        return false;
    }

    VolumeRecord winner = LoadVolume( Parameters.Volumes, mediumIndex );
    float extinction = winner.AbsorptionDensity[channel] + winner.ScatteringPhase[channel];

    // Density multiplies absorption and scattering equally, so it cancels in this choice.
    if ( Random( random ) * extinction < winner.AbsorptionDensity[channel] ) {
        weight = 0.0;
        return false;
    }

    return true;
}

// Beer transmittance is exact for analytic fog; ratio tracking is only necessary
// for procedural/grid density. Independent media multiply their transmittances.
// https://pbr-book.org/4ed/Volume_Scattering/Transmittance
// https://pbr-book.org/4ed/Light_Transport_II_Volume_Rendering/Volume_Scattering_Integrators#VolPathIntegrator::SampleLd
float3 SceneFogTransmittance( float3 origin, float3 direction, float maximum, inout uint random ) {
    float3 transmittance = 1.0;
    uint count = VolumeCount( Parameters.Volumes );
    bool heterogeneous = false;

    for ( uint index = 0; index < count; ++index ) {
        VolumeRecord medium = LoadVolume( Parameters.Volumes, index );
        float entry, exit;

        if ( medium.Flags.z == 0 || !any( medium.ExtinctionMajorant.xyz > 0.0 ) ||
             !VolumeRayInterval( medium, origin, direction, entry, exit ) ) {
            continue;
        }

        entry = max( entry, 0.0 );
        exit = min( exit, maximum );

        if ( exit <= entry ) {
            continue;
        }

        if ( medium.Flags.y > 1 ) {
            heterogeneous = true;
            continue;
        }

        float integral = VolumeAnalyticIntegral( MakeVolumeAnalyticProfile( medium, origin, direction, entry, exit ) );
        transmittance *= exp( -medium.ExtinctionMajorant.xyz * integral );
    }

    if ( !heterogeneous || !any( transmittance > 0.0 ) ) {
        return transmittance;
    }

    for ( uint index = 0; index < count; ++index ) {
        VolumeRecord medium = LoadVolume( Parameters.Volumes, index );
        float entry, exit;

        if ( medium.Flags.z == 0 || medium.Flags.y <= 1 || !any( medium.ExtinctionMajorant.xyz > 0.0 ) ||
             !VolumeRayInterval( medium, origin, direction, entry, exit ) ) {
            continue;
        }

        float majorant = MaxComponent( medium.ExtinctionMajorant.xyz ) * 1.00001;
        float3 coefficient = medium.AbsorptionDensity.xyz + medium.ScatteringPhase.xyz;
        float t = max( entry, 0.0 );
        exit = min( exit, maximum );

        while ( t < exit ) {
            t += -log( max( 1.0 - Random( random ), 1e-7 ) ) / majorant;

            if ( t >= exit ) {
                break;
            }

            float density = VolumeDensity( Parameters.Volumes, medium, origin + direction * t, false );
            transmittance *= max( 0.0, 1.0 - coefficient * density / majorant );
            float survive = MaxComponent( transmittance );

            if ( survive < 0.1 ) {
                if ( survive <= 0.0 || Random( random ) >= survive ) {
                    return 0.0;
                }

                transmittance /= survive;
            }
        }
    }

    return transmittance;
}

float ScatteringCosine( Surface surface, float3 direction ) {
    return surface.Model == MATERIAL_MEDIUM ? 1.0 : abs( dot( surface.Normal, direction ) );
}

// Connection rays cross only null interfaces. Refractive glass is a true specular
// boundary and blocks this straight connection, just as in a unidirectional path tracer.
float3 ShadowTransmittance( Surface surface, float3 direction, float maximum, MediumStack media, inout uint random ) {
    if ( surface.Model == MATERIAL_DIELECTRIC && !surface.ThinWalled && dot( surface.GeometricNormal, direction ) < 0.0 ) {
        if ( !CrossMediumBoundary( media, surface ) ) {
            return float3( 0 );
        }
    }
    float3 origin = Offset( surface, direction );
    float3 transmittance = float3( 1 );

    for ( uint crossing = 0; crossing < 256; ++crossing ) {
        Surface boundary;
        Hit hit;
        bool found = Intersect( origin, direction, maximum, random, boundary, hit, false, true );

        // A real occluder makes the connection zero regardless of its fog transmittance.
        if ( found && !NullMediumBoundary( boundary, media ) ) {
            return float3( 0 );
        }

        float distance = found ? hit.Distance : maximum;
        MediumState medium = ReadMedium( media, media.Count );
        transmittance *= exp( -( medium.Absorption + medium.Scattering ) * distance );
        if ( media.Count == 0 && VolumeCount( Parameters.Volumes ) != 0 ) {
            transmittance *= SceneFogTransmittance( origin, direction, distance, random );
        }
        if ( !found ) {
            return transmittance;
        }
        if ( !CrossMediumBoundary( media, boundary ) ) {
            return float3( 0 );
        }
        float3 next = Offset( boundary, direction );
        maximum = max( 0.0, maximum - dot( next - origin, direction ) );
        origin = next;
    }
    return float3( 0 );
}

float3 ShadowTransmittanceTo( Surface surface, float3 target, MediumStack media, inout uint random ) {
    float3 origin = Offset( surface, target - surface.Position );
    float3 delta = target - origin;
    float distance = length( delta );
    return distance > 0.0 ? ShadowTransmittance( surface, delta / distance, NextFloat( distance, false ), media, random ) : float3( 0 );
}
