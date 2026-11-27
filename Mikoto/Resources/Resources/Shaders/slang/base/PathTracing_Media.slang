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

// PBRT v3 homogeneous-medium code:
// Copyright (c) 1998-2016 Matt Pharr, Greg Humphreys, and Wenzel Jakob.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// - Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// - Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

// Track active media, sample scattering events and evaluate shadow transmittance. Surface
// boundary crossings update the medium state used for the next ray segment.

#pragma once

#include "VolumeTracking_Helpers.slang"

// Homogeneous-medium distance sampling and its RGB channel-mixture density follow PBRT.
// https://pbr-book.org/3ed-2018/Volume_Scattering/Media
// https://github.com/mmp/pbrt-v3/blob/master/src/media/homogeneous.cpp
// Henyey-Greenstein convention/sampling follows PBRT v4 (both directions point away).
// https://pbr-book.org/4ed/Volume_Scattering/Phase_Functions
// https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/util/sampling.cpp

// Resolve the optical coefficients of the selected nested medium. An empty stack represents the
// exterior with unit refractive index.
MediumState ReadMedium( MediumStack media, uint depth ) {
    // An empty stack represents the exterior, which has an index of refraction of one.
    MediumState result = ( MediumState )0;
    result.mIOR = 1.0;
    if ( depth > 0 ) {
        MaterialRecord material = LoadMaterialRecord( media.mMaterials[depth - 1] );
        result.mAbsorption = material.mAbsorption.xyz;
        result.mScattering = material.mScattering.xyz;
        result.mAnisotropy = material.mAbsorption.w;
        result.mIOR = material.mOptical.x;
    }
    return result;
}

// Push the entered material or pop the exited medium. This stack is path-local and changes only
// when a boundary is actually crossed.
bool CrossMediumBoundary( inout MediumStack media, Surface boundary ) {
    if ( boundary.mFrontFace ) {
        if ( media.mCount == kPathMediumStackCapacity ) {
            return false;
        }
        media.mMaterials[media.mCount++] = boundary.mMaterial;
    }
    else if ( media.mCount > 0 ) {
        --media.mCount;
    }
    return true;
}

// Recognize an interface that changes medium bookkeeping without a refractive event. Such
// crossings do not consume the surface-scattering bounce budget.
bool NullMediumBoundary( Surface surface, MediumStack media ) {
    uint outside = surface.mFrontFace ? media.mCount : ( media.mCount > 1 ? media.mCount - 1 : 0 );
    return surface.mModel == MATERIAL_DIELECTRIC && !surface.mThinWalled &&
           surface.mIOR == ReadMedium( media, outside ).mIOR && surface.mTransmission == 1.0;
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
                       0.5 * g * g * g * ( k * k - 1.0 ) ) /
                   ( denominator * denominator );
    return Hemisphere( view, clamp( cosine, -1.0, 1.0 ), 2.0 * PI * sample.y );
}

// Sample free flight from one RGB extinction channel and weight with the channel-mixture PDF.
// Both collision and no-collision outcomes retain their probability correction.
bool SampleMedium( MediumState medium, float maximum, float2 sample, out float distance, out float3 weight ) {
    distance = maximum;
    if ( !any( medium.mScattering > 0.0 ) ) {
        // Pure absorption has an exact deterministic solution and needs no random walk.
        weight = exp( -medium.mAbsorption * maximum );
        return false;
    }
    float3 extinction = medium.mAbsorption + medium.mScattering;
    uint channel = min( uint( sample.x * 3.0 ), 2u );
    float freeFlight = extinction[channel] > 0.0 ? -log( 1.0 - sample.y ) / extinction[channel] : RAY_MAX_DISTANCE;
    bool scatter = freeFlight < maximum;
    distance = min( freeFlight, maximum );
    float3 transmittance = exp( -extinction * distance );
    float3 density = scatter ? extinction * transmittance : transmittance;
    float pdf = ( density.x + density.y + density.z ) / 3.0;
    weight = pdf > 0.0 ? transmittance * ( scatter ? medium.mScattering : float3( 1 ) ) / pdf : float3( 0 );
    return scatter;
}

// Weighted null-collision transport retains all RGB channels. Scatter,
// absorption and null events use their average RGB probabilities; the chosen
// event contributes its RGB probability divided by that scalar probability.
// Empty density therefore has a unit weight, even inside a chromatic grid box.
// https://jannovak.info/publications/SDTracking/index.html
// Find the earliest candidate among overlapping scene volumes, then classify it as scattering,
// absorption or a null event. Majorants bound density while null events preserve unbiased
// continuation.
bool SampleSceneFog( float3 origin, float3 direction, float maximum, inout uint random,
    out float distance, out float3 weight, out uint mediumIndex ) {
    distance = maximum;
    weight = 1.0;
    mediumIndex = 0xffffffffu;
    uint count = VolumeCount( Parameters.mVolumes );
    float cursor = 0;

    while ( cursor < maximum ) {
        float nearest = maximum;
        float majorant = 0;
        uint candidateIndex = 0xffffffffu;
        for ( uint index = 0; index < count; ++index ) {
            VolumeRecord medium = LoadVolume( Parameters.mVolumes, index );
            float entry;
            float exit;
            if ( !any( medium.mExtinctionMajorant.xyz > 0 ) ||
                 !VolumeRayInterval( medium, origin, direction, entry, exit ) ) {
                continue;
            }
            entry = max( entry, cursor );
            exit = min( exit, nearest );
            if ( exit <= entry ) {
                continue;
            }

            float candidate;
            float bound;
            if ( SampleVolumeCandidate( Parameters.mVolumes, medium, origin, direction, entry, exit,
                     random, candidate, bound ) ) {
                nearest = candidate;
                majorant = bound;
                candidateIndex = index;
            }
        }
        if ( candidateIndex == 0xffffffffu ) {
            return false;
        }

        VolumeRecord medium = LoadVolume( Parameters.mVolumes, candidateIndex );
        float3 position = origin + direction * nearest;
        float density = VolumeDensity( Parameters.mVolumes, medium, position, false );
        if ( medium.mFlags.y <= 1 ) {
            // Analytic height sampling/transmittance use the same unclipped
            // exponential profile; its local majorant already includes density.
            float3 coefficient = medium.mAbsorptionDensity.xyz + medium.mScatteringPhase.xyz;
            density = majorant / max( coefficient.x, max( coefficient.y, coefficient.z ) );
        }
        float3 scattering = medium.mScatteringPhase.xyz * density / majorant;
        float3 absorption = medium.mAbsorptionDensity.xyz * density / majorant;
        float scatterProbability = ( scattering.x + scattering.y + scattering.z ) / 3.0;
        float absorbProbability = ( absorption.x + absorption.y + absorption.z ) / 3.0;
        float choice = Random( random );
        if ( choice < scatterProbability ) {
            weight *= scattering / scatterProbability;
            distance = nearest;
            mediumIndex = candidateIndex;
            return true;
        }
        if ( choice < scatterProbability + absorbProbability ) {
            weight = 0;
            distance = nearest;
            return false;
        }
        float nullProbability = 1.0 - scatterProbability - absorbProbability;
        weight *= max( 0.0, 1.0 - scattering - absorption ) / nullProbability;
        if ( !any( weight > 0 ) ) {
            return false;
        }
        // Only the globally nearest candidate may change the weight. Advancing
        // all constituents together avoids weighting beyond another's event.
        cursor = asfloat( asuint( nearest ) + 1u );
    }
    return false;
}

// Beer transmittance is exact for analytic fog; ratio tracking is only necessary
// for procedural/grid density. Independent media multiply their transmittances.
// https://pbr-book.org/4ed/Volume_Scattering/Transmittance
// https://pbr-book.org/4ed/Light_Transport_II_Volume_Rendering/Volume_Scattering_Integrators#VolPathIntegrator::SampleLd
// Multiply each intersected volume transmittance estimate along the finite segment. Volumes
// disabled for shadow transport are skipped.
float3 SceneFogTransmittance( float3 origin, float3 direction, float maximum, inout uint random ) {
    float3 transmittance = 1.0;
    uint count = VolumeCount( Parameters.mVolumes );
    for ( uint index = 0; index < count; ++index ) {
        VolumeRecord medium = LoadVolume( Parameters.mVolumes, index );
        float entry;
        float exit;
        if ( medium.mFlags.z == 0 || !any( medium.mExtinctionMajorant.xyz > 0 ) ||
             !VolumeRayInterval( medium, origin, direction, entry, exit ) ) {
            continue;
        }
        entry = max( entry, 0.0 );
        exit = min( exit, maximum );
        if ( exit <= entry ) {
            continue;
        }
        transmittance *= TrackVolumeTransmittance( Parameters.mVolumes, medium, origin, direction, entry, exit, random );
        if ( !any( transmittance > 0 ) ) {
            return 0;
        }
    }

    return transmittance;
}

float ScatteringCosine( Surface surface, float3 direction ) {
    return surface.mModel == MATERIAL_MEDIUM ? 1.0 : abs( dot( surface.mNormal, direction ) );
}

// Connection rays cross only null interfaces. Refractive glass is a true specular
// boundary and blocks this straight connection, just as in a unidirectional path tracer.
// Follow a shadow segment through transparent null boundaries and active media. Opaque/non-null
// boundaries block the connection; medium extinction attenuates each traversed interval.
float3 ShadowTransmittance( Surface surface, float3 direction, float maximum, MediumStack media, inout uint random ) {
    if ( surface.mModel == MATERIAL_DIELECTRIC && !surface.mThinWalled && dot( surface.mGeometricNormal, direction ) < 0.0 ) {
        if ( !CrossMediumBoundary( media, surface ) ) {
            return float3( 0 );
        }
    }
    float3 origin = Offset( surface, direction );
    float3 transmittance = float3( 1 );

    for ( uint crossing = 0; crossing < kPathBoundaryCrossingLimit; ++crossing ) {
        Surface boundary;
        Hit hit;
        bool found = Intersect( origin, direction, maximum, random, boundary, hit, false, true );

        // A real occluder makes the connection zero regardless of its fog transmittance.
        if ( found && !NullMediumBoundary( boundary, media ) ) {
            return float3( 0 );
        }

        float distance = found ? hit.mDistance : maximum;
        MediumState medium = ReadMedium( media, media.mCount );
        transmittance *= exp( -( medium.mAbsorption + medium.mScattering ) * distance );
        if ( media.mCount == 0 && VolumeCount( Parameters.mVolumes ) != 0 ) {
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
    float3 origin = Offset( surface, target - surface.mPosition );
    float3 delta = target - origin;
    float distance = length( delta );
    return distance > 0.0 ? ShadowTransmittance( surface, delta / distance, NextFloat( distance, false ), media, random ) : float3( 0 );
}
