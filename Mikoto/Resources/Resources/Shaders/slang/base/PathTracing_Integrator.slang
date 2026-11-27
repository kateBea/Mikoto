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

// Iterate surface and medium scattering while accumulating emitted and sampled direct radiance.
// Throughput, path PDFs and termination decisions carry energy between bounces.

#pragma once

// Iterative PBRT-style light transport using the hardware hit stages.

// Choose the visible camera background independently from indirect environment lighting. A
// scene can use a flat/hidden background while retaining environment illumination.
float3 CameraBackground( float3 direction, inout uint random ) {
    float3 background = Effects().mBackground.xyz;
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

// Apply Russian roulette after the initial bounces. Surviving paths divide throughput by their
// survival probability; eta scaling avoids prematurely killing refracted paths.
bool ContinuePath( inout float3 throughput, float etaScale, uint depth, inout uint random ) {
    float survive = MaxComponent( throughput * etaScale );
    if ( survive <= 0.0 ) {
        return false;
    }
    if ( depth > kPathRouletteStartDepth && survive < 1.0 ) {
        if ( Random( random ) >= survive ) {
            return false;
        }
        throughput /= survive;
    }
    return true;
}

// Advance through surface hits and medium events until termination. The first-event distance is
// exported alongside radiance for denoiser hit-distance guides.
float3 IntegrateWithHitDistance( float3 origin, float3 direction, inout uint random, uint maxDepth,
    bool cameraPath, PreviousVertex previous, out float firstHitDistance ) {
    firstHitDistance = 60000.0;
    float3 radiance = float3( 0.0 );
    float3 throughput = float3( 1.0 );
    float etaScale = 1.0;
    MediumStack media = ( MediumStack )0;
    uint depth = 0;
    uint nullCrossings = 0;

    while ( true ) {
        // Find the surface first so medium sampling can stop at its boundary.
        Surface s;
        Hit hit;
        bool found = Intersect( origin, direction, RAY_MAX_DISTANCE, random, s, hit );
        if ( depth == 0 && nullCrossings == 0 ) {
            firstHitDistance = found ? min( hit.mDistance, 60000.0 ) : 60000.0;
        }
        MediumState medium = ReadMedium( media, media.mCount );
        float distance;
        float3 mediumWeight;
        uint fogMedium = 0xffffffffu;
        bool scattered;
        if ( media.mCount == 0 && VolumeCount( Parameters.mVolumes ) != 0 ) {
            scattered = SampleSceneFog( origin, direction, found ? hit.mDistance : RAY_MAX_DISTANCE, random,
                distance, mediumWeight, fogMedium );
        }
        else {
            float2 mediumSample = Random2D( random );
            scattered = SampleMedium( medium, found ? hit.mDistance : RAY_MAX_DISTANCE, mediumSample, distance, mediumWeight );
        }
        throughput *= mediumWeight;
        if ( !any( throughput > 0.0 ) ) {
            break;
        }

        // Do not run the existing planetary-air integral through glass interiors.
        if ( HasAtmosphere() && media.mCount == 0 && ( found || scattered ) ) {
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
            event.mModel = MATERIAL_MEDIUM;
            event.mPosition = origin + distance * direction;
            event.mAnisotropy = medium.mAnisotropy;
            bool receiveShadows = true;
            if ( fogMedium != 0xffffffffu ) {
                VolumeRecord fog = LoadVolume( Parameters.mVolumes, fogMedium );
                event.mAnisotropy = fog.mScatteringPhase.w;
                receiveShadows = fog.mFlags.w != 0;
            }
            radiance += throughput * SampleDirect( event, -direction, random, ALL_LOBES, true, media, receiveShadows );
            BSDFSample phase = SampleBSDF( event, -direction, random );
            if ( phase.mPDF <= 0.0 ) {
                break;
            }
            throughput *= phase.mValue / phase.mPDF;
            previous.mPosition = event.mPosition;
            previous.mBSDFPDF = phase.mPDF;
            previous.mLightCount = LightCount();
            origin = event.mPosition;
            direction = phase.mDirection;
            if ( !ContinuePath( throughput, etaScale, ++depth, random ) ) {
                break;
            }
            continue;
        }

        // Sky and emitter hits use the previous vertex's PDF for MIS.
        if ( !found ) {
            bool background = cameraPath && depth == 0;
            float weight = previous.mBSDFPDF > 0.0 && previous.mLightCount > 0
                               ? PowerWeight( previous.mBSDFPDF, EnvironmentPDF( previous.mLightCount ) )
                               : 1.0;
            radiance += throughput * weight * ( background ? CameraBackground( direction, random ) : Environment( direction ) );
            break;
        }

        // An IOR-one boundary only changes the medium; it consumes no scattering depth.
        // A null boundary updates the active medium without consuming a scattering bounce. The
        // crossing limit bounds malformed or deeply nested geometry traversal.
        if ( NullMediumBoundary( s, media ) ) {
            if ( ++nullCrossings > kPathBoundaryCrossingLimit || !CrossMediumBoundary( media, s ) ) {
                break;
            }
            origin = Offset( s, direction );
            continue;
        }

        // Account for emitter hits using the previous scattering PDF. Multiple-importance
        // weighting balances this path against explicit light sampling.
        if ( any( s.mEmission > 0.0 ) ) {
            float weight = previous.mBSDFPDF > 0.0 && previous.mLightCount > 0
                               ? PowerWeight( previous.mBSDFPDF, EmitterPDF( hit.mTriangle, s, previous.mPosition, previous.mLightCount ) )
                               : 1.0;
            radiance += throughput * s.mEmission * weight;
        }
        // A terminal ray still sees emission, but does not create another scattering event.
        if ( depth >= maxDepth || s.mEmissionOnly ) {
            break;
        }

        float materialIOR = s.mIOR;
        if ( s.mModel == MATERIAL_DIELECTRIC && !s.mThinWalled ) {
            float outside = ReadMedium( media, s.mFrontFace ? media.mCount : ( media.mCount > 1 ? media.mCount - 1 : 0 ) ).mIOR;
            s.mIOR = materialIOR / outside;
        }
        radiance += throughput * SampleDirect( s, -direction, random, ALL_LOBES, true, media );
        BSDFSample sample = SampleBSDF( s, -direction, random );
        if ( sample.mPDF <= 0.0 || !any( sample.mValue > 0.0 ) ) {
            break;
        }
        throughput *= sample.mValue * abs( dot( s.mNormal, sample.mDirection ) ) / sample.mPDF;
        // Transmission may change the active medium or select a subsurface exit point. Update
        // that state before offsetting and continuing the ray.
        if ( sample.mTransmitted ) {
            etaScale *= sample.mEta * sample.mEta;
            if ( s.mModel == MATERIAL_SUBSURFACE ) {
                Surface exit;
                float3 weight;
                if ( !SampleBssrdf( s, random, exit, weight ) ) {
                    break;
                }
                throughput *= weight;
                s = exit;
                radiance += throughput * SampleDirect( s, s.mNormal, random, ALL_LOBES, true, media );
                sample = SampleBSDF( s, s.mNormal, random );
                if ( sample.mPDF <= 0.0 ) {
                    break;
                }
                throughput *= sample.mValue * abs( dot( s.mNormal, sample.mDirection ) ) / sample.mPDF;
                etaScale /= s.mIOR * s.mIOR;
            }
            else if ( s.mThinWalled ) {
                // ThinDielectric has no interior segment; thickness supplies optional absorption.
                throughput *= exp( -s.mAbsorption * s.mThickness / max( abs( dot( s.mNormal, sample.mDirection ) ), 1e-6 ) );
            }
            else if ( s.mModel == MATERIAL_DIELECTRIC ) {
                if ( !CrossMediumBoundary( media, s ) ) {
                    break;
                }
            }
        }

        // Russian roulette preserves expected radiance by compensating survivors.
        if ( !ContinuePath( throughput, etaScale, ++depth, random ) ) {
            break;
        }

        previous.mPosition = s.mPosition;
        previous.mBSDFPDF = sample.mDelta ? 0.0 : sample.mPDF;
        previous.mLightCount = LightCount();
        origin = Offset( s, sample.mDirection );
        direction = sample.mDirection;
    }
    return radiance;
}

float3 Integrate( float3 origin, float3 direction, inout uint random, uint maxDepth,
    bool cameraPath, PreviousVertex previous ) {
    float unused;
    return IntegrateWithHitDistance( origin, direction, random, maxDepth, cameraPath, previous, unused );
}

// Start an indirect-only transport estimate from an already evaluated surface. Light sampling
// and BSDF continuation share the requested lobe selection.
float3 Indirect( Surface s, float3 view, inout uint random, uint lobes, uint maxDepth ) {
    // Raster already supplies punctual direct lighting. Sample only emitters/sky
    // here, then hand the exact first-lobe PDF to the shared transport loop.
    if ( lobes == DIFFUSE && ( s.mMetallic >= 1.0 || !any( s.mBase > 0.0 ) ) ) {
        return float3( 0.0 );
    }

    float3 radiance = SampleDirect( s, view, random, lobes, false );
    BSDFSample sample = SampleBSDF( s, view, random, lobes );
    if ( sample.mPDF > 0.0 && any( sample.mValue > 0.0 ) ) {
        PreviousVertex previous;
        previous.mPosition = s.mPosition;
        previous.mBSDFPDF = sample.mPDF;
        previous.mLightCount = LightCount( false );
        radiance += Integrate( Offset( s, sample.mDirection ), sample.mDirection, random, maxDepth, false, previous ) *
                    sample.mValue * saturate( dot( s.mNormal, sample.mDirection ) ) / sample.mPDF;
    }
    return radiance;
}
