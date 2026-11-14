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

// Iterative PBRT-style light transport using the hardware hit stages.

float3 CameraBackground( float3 direction, inout uint random ) {
    float3 background = Effects().Background.xyz;
    if ( background.z != 0.0 ) {
        return float3( 0.0 );
    }
    if ( background.x > 0.0 ) {
        // Filter radiance directions only after a camera miss: silhouettes and
        // secondary-ray illumination must not inherit this artistic blur.
        // Progressive accumulation averages the cone samples without allocating
        // raster IBL targets. Prefilter mode uses a roughness-like angular width;
        // ordinary sky mode approximates a cubemap mip's growing footprint.
        float angle = background.y != 0.0
                          ? pow( saturate( background.x / 8.0 ), 2.0 ) * ( PI * 0.5 )
                          : min( ( exp2( background.x ) - 1.0 ) / 512.0, PI * 0.5 );
        float cosine = lerp( 1.0, cos( angle ), Random( random ) );
        direction = Hemisphere( direction, cosine, Random( random ) * 2.0 * PI );
    }
    return Environment( direction, true );
}

// PBRT's PathIntegrator::Li, adapted to RGB and hardware TraceRay intersections.
// Keep the preceding vertex's sampling context for BOTH emitter hits and sky misses.
// https://pbr-book.org/4ed/Light_Transport_I_Surface_Reflection/A_Better_Path_Tracer
#include "PathTracing_Bssrdf.slang"

bool ContinuePath( inout float3 throughput, float etaScale, uint depth, inout uint random ) {
    float survive = MaxComponent( throughput * etaScale );
    if ( survive <= 0.0 ) {
        return false;
    }
    if ( depth > 2 && survive < 1.0 ) {
        if ( Random( random ) >= survive ) {
            return false;
        }
        throughput /= survive;
    }
    return true;
}

float3 Integrate( float3 origin, float3 direction, inout uint random, uint maxDepth,
    bool cameraPath, PreviousVertex previous ) {
    float3 radiance = float3( 0.0 );
    float3 throughput = float3( 1.0 );
    float etaScale = 1.0;
    MediumStack media = ( MediumStack )0;
    uint depth = 0, nullCrossings = 0;
    int fogChannel = -1;

    while ( true ) {
        // Find the surface first so medium sampling can stop at its boundary.
        Surface s;
        Hit hit;
        bool found = Intersect( origin, direction, RAY_MAX_DISTANCE, random, s, hit );
        MediumState medium = ReadMedium( media, media.Count );
        float distance;
        float3 mediumWeight;
        uint fogMedium = 0xffffffffu;
        bool scattered;
        if ( media.Count == 0 && VolumeCount( Parameters.Volumes ) != 0 ) {
            scattered = SampleSceneFog( origin, direction, found ? hit.Distance : RAY_MAX_DISTANCE, random,
                fogChannel, distance, mediumWeight, fogMedium );
        }
        else {
            float2 mediumSample = Random2D( random );
            scattered = SampleMedium( medium, found ? hit.Distance : RAY_MAX_DISTANCE, mediumSample, distance, mediumWeight );
        }
        throughput *= mediumWeight;
        if ( !any( throughput > 0.0 ) ) {
            break;
        }

        // Do not run the existing planetary-air integral through glass interiors.
        if ( HasAtmosphere() && media.Count == 0 && ( found || scattered ) ) {
            AtmosphereData atmosphere = Atmosphere();
            if ( atmosphere.mView.y > 0.5 ) {
                float3 planetOrigin = AtmosWorldOrigin( atmosphere, origin );
                float entry = max( 0, AtmosEntryDistance( atmosphere, planetOrigin, direction ) );
                float range = min( distance * atmosphere.mRadii.z, entry + atmosphere.mGround.w );
                AtmosIntegration air = AtmosIntegrate( atmosphere, planetOrigin, direction,
                    atmosphere.mSunDirection.xyz, range, 16, true, false, false );
                radiance += throughput * air.mRadiance * atmosphere.mSunIlluminance.xyz;
                throughput *= air.mTransmittance;
            }
        }

        // A medium event consumes one bounce and samples a phase function.
        if ( scattered ) {
            if ( depth >= maxDepth ) {
                break;
            }
            Surface event = ( Surface )0;
            event.Model = MATERIAL_MEDIUM;
            event.Position = origin + distance * direction;
            event.Anisotropy = medium.Anisotropy;
            bool receiveShadows = true;
            if ( fogMedium != 0xffffffffu ) {
                VolumeRecord fog = LoadVolume( Parameters.Volumes, fogMedium );
                event.Anisotropy = fog.ScatteringPhase.w;
                receiveShadows = fog.Flags.w != 0;
            }
            radiance += throughput * SampleDirect( event, -direction, random, ALL_LOBES, receiveShadows, media );
            BSDFSample phase = SampleBSDF( event, -direction, random );
            if ( phase.PDF <= 0.0 ) {
                break;
            }
            throughput *= phase.Value / phase.PDF;
            previous.Position = event.Position;
            previous.BSDFPDF = phase.PDF;
            previous.LightCount = LightCount();
            origin = event.Position;
            direction = phase.Direction;
            if ( !ContinuePath( throughput, etaScale, ++depth, random ) ) {
                break;
            }
            continue;
        }

        // Sky and emitter hits use the previous vertex's PDF for MIS.
        if ( !found ) {
            bool background = cameraPath && depth == 0;
            float weight = previous.BSDFPDF > 0.0 && previous.LightCount > 0
                               ? PowerWeight( previous.BSDFPDF, EnvironmentPDF( previous.LightCount ) )
                               : 1.0;
            radiance += throughput * weight * ( background ? CameraBackground( direction, random ) : Environment( direction ) );
            break;
        }

        // An IOR-one boundary only changes the medium; it consumes no scattering depth.
        if ( NullMediumBoundary( s, media ) ) {
            if ( ++nullCrossings > 256 || !CrossMediumBoundary( media, s ) ) {
                break;
            }
            origin = Offset( s, direction );
            continue;
        }

        if ( any( s.Emission > 0.0 ) ) {
            float weight = previous.BSDFPDF > 0.0 && previous.LightCount > 0
                ? PowerWeight( previous.BSDFPDF, EmitterPDF( hit.Triangle, s, previous.Position, previous.LightCount ) ) : 1.0;
            radiance += throughput * s.Emission * weight;
        }
        // A terminal ray still sees emission, but does not create another scattering event.
        if ( depth >= maxDepth || s.EmissionOnly ) {
            break;
        }

        float materialIOR = s.IOR;
        if ( s.Model == MATERIAL_DIELECTRIC && !s.ThinWalled ) {
            float outside = ReadMedium( media, s.FrontFace ? media.Count : ( media.Count > 1 ? media.Count - 1 : 0 ) ).IOR;
            s.IOR = materialIOR / outside;
        }
        radiance += throughput * SampleDirect( s, -direction, random, ALL_LOBES, true, media );
        BSDFSample sample = SampleBSDF( s, -direction, random );
        if ( sample.PDF <= 0.0 || !any( sample.Value > 0.0 ) ) {
            break;
        }
        throughput *= sample.Value * abs( dot( s.Normal, sample.Direction ) ) / sample.PDF;
        if ( sample.Transmitted ) {
            etaScale *= sample.Eta * sample.Eta;
            if ( s.Model == MATERIAL_SUBSURFACE ) {
                Surface exit;
                float3 weight;
                if ( !SampleBssrdf( s, random, exit, weight ) ) {
                    break;
                }
                throughput *= weight;
                s = exit;
                radiance += throughput * SampleDirect( s, s.Normal, random, ALL_LOBES, true, media );
                sample = SampleBSDF( s, s.Normal, random );
                if ( sample.PDF <= 0.0 ) {
                    break;
                }
                throughput *= sample.Value * abs( dot( s.Normal, sample.Direction ) ) / sample.PDF;
                etaScale /= s.IOR * s.IOR;
            }
            else if ( s.ThinWalled ) {
                // ThinDielectric has no interior segment; thickness supplies optional absorption.
                throughput *= exp( -s.Absorption * s.Thickness / max( abs( dot( s.Normal, sample.Direction ) ), 1e-6 ) );
            }
            else if ( s.Model == MATERIAL_DIELECTRIC ) {
                if ( !CrossMediumBoundary( media, s ) ) {
                    break;
                }
            }
        }

        // Russian roulette preserves expected radiance by compensating survivors.
        if ( !ContinuePath( throughput, etaScale, ++depth, random ) ) {
            break;
        }

        previous.Position = s.Position;
        previous.BSDFPDF = sample.Delta ? 0.0 : sample.PDF;
        previous.LightCount = LightCount();
        origin = Offset( s, sample.Direction );
        direction = sample.Direction;
    }
    return radiance;
}

float3 Indirect( Surface s, float3 view, inout uint random, uint lobes, uint maxDepth ) {
    // Raster already supplies punctual direct lighting. Sample only emitters/sky
    // here, then hand the exact first-lobe PDF to the shared transport loop.
    if ( lobes == DIFFUSE && ( s.Metallic >= 1.0 || !any( s.Base > 0.0 ) ) ) {
        return float3( 0.0 );
    }

    float3 radiance = SampleDirect( s, view, random, lobes, false );
    BSDFSample sample = SampleBSDF( s, view, random, lobes );
    if ( sample.PDF > 0.0 && any( sample.Value > 0.0 ) ) {
        PreviousVertex previous;
        previous.Position = s.Position;
        previous.BSDFPDF = sample.PDF;
        previous.LightCount = LightCount( false );
        radiance += Integrate( Offset( s, sample.Direction ), sample.Direction, random, maxDepth, false, previous ) *
                    sample.Value * saturate( dot( s.Normal, sample.Direction ) ) / sample.PDF;
    }
    return radiance;
}
