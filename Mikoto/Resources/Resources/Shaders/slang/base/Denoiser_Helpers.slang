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

// Generate NRD surface guides and separated diffuse/specular radiance signals. Unsupported
// media/transmission paths keep their base radiance instead of inventing opaque-surface guides.

#pragma once

#if MKT_NRD_AVAILABLE
#include "NRD.hlsli"

// Exact front-end encodings from the pinned NRD SDK; embedded shaders use the same
// oct-normal/linear-roughness configuration. Guides use FP16 storage; lights are scene-linear.
struct DenoiserRayData {
    float4x4 mWorldToView;
    uint4 mImages0;
    uint4 mImages1;
    uint64_t mPreviousPositions;
    uint mEnabled;
    uint mMethod;
    float4 mJitter;
};

DenoiserRayData Denoising() {
    return LoadBufferValue<DenoiserRayData>( Parameters.mDenoiser, 0 );
}

// An unrelated sky cloud does not invalidate the guide for a nearby solid.
// Reject only the camera-to-surface segment that actually enters a medium.
// Check whether a nonempty volume intersects the primary ray before its first surface. Opaque
// NRD surface guides cannot describe this volumetric segment.
bool DenoiserPrimaryMedium( float3 origin, float3 direction, float maximum ) {
    uint count = VolumeCount( Parameters.mVolumes );
    for ( uint index = 0; index < count; ++index ) {
        VolumeRecord medium = LoadVolume( Parameters.mVolumes, index );
        float entry;
        float exit;
        if ( any( medium.mExtinctionMajorant.xyz > 0 ) &&
            VolumeRayInterval( medium, origin, direction, entry, exit ) &&
            min( exit, maximum ) > max( entry, 0.0 ) ) {
            return true;
        }
    }
    return false;
}

// Produce surface guides, motion and separated radiance for NRD, or retain a progressive
// camera-path estimate for unsupported transport. The base-image validity flag selects the
// later reconstruction path.
void RenderDenoiserInputs( uint2 pixel ) {
    DenoiserRayData nrd = Denoising();
    // Finite out-of-range depth excludes unsupported pixels from surface denoising.
    RayImages[nrd.mImages0.x][pixel] = 0;
    RayImages[nrd.mImages0.y][pixel] = 0;
    RayImages[nrd.mImages0.z][pixel] = float4( 0, 0, 1, 1 );
    RayImages[nrd.mImages0.w][pixel] = float4( 1000000, 0, 0, 0 );
    RayImages[nrd.mImages1.x][pixel] = 0;
    RayImages[nrd.mImages1.y][pixel] = 1;
    RayImages[nrd.mImages1.z][pixel] = 1;
    RayImages[nrd.mImages1.w][pixel] = 0;
    bool path = Parameters.mFrame.w != 0;
    uint random = PixelSampleSeed( pixel, 0 );
    float3 origin;
    float3 direction;
    GenerateCameraRay( pixel, .5 + ( path ? nrd.mJitter.xy : float2( 0 ) ), origin, direction );
    Surface s;
    Hit hit;
    bool found = Intersect( origin, direction, RAY_MAX_DISTANCE, random, s, hit, !path );
    bool primaryMedium = path && DenoiserPrimaryMedium( origin, direction, found ? hit.mDistance : RAY_MAX_DISTANCE );

    // A single opaque guide cannot describe stochastic medium, refraction or thin-lens
    // paths. Keep the existing transport for these pixels, without smearing their silhouettes.
    // Classify whether this pixel has a valid single-surface denoising contract. Refraction,
    // subsurface paths, lens blur and primary media require different guide information.
    bool supported = nrd.mEnabled != 0 && found && s.mModel != MATERIAL_DIELECTRIC && s.mModel != MATERIAL_SUBSURFACE &&
                     s.mModel != MATERIAL_UNLIT && !s.mEmissionOnly && s.mTransmission == 0 &&
                     ( !path || ( !primaryMedium && Effects().mLensRight.w == 0 ) );
    // Unsupported pixels keep their depth out of NRD's denoising range.
    if ( found ) {
        RayImages[nrd.mImages0.z][pixel] = NRD_FrontEnd_PackNormalAndRoughness( s.mNormal, s.mRoughness, 0 );
        uint3 indices = LoadTriangleIndices( hit.mTriangle );
        float3 bary = float3( 1 - hit.mBarycentrics.x - hit.mBarycentrics.y, hit.mBarycentrics );
        float3 previousPosition = LoadBufferValue<float4>( nrd.mPreviousPositions, indices.x * 16 ).xyz * bary.x +
            LoadBufferValue<float4>( nrd.mPreviousPositions, indices.y * 16 ).xyz * bary.y +
            LoadBufferValue<float4>( nrd.mPreviousPositions, indices.z * 16 ).xyz * bary.z;
        RayImages[nrd.mImages1.x][pixel] = float4( previousPosition - s.mPosition, 0 );
    }

    // Camera fallbacks and both filtered lobes share one transport call site.
    // Keeping independent RenderPathTracing/RenderHybridLighting calls here
    // cloned the entire integrator and stalled Vulkan pipeline creation.
    bool cameraPath = path && !supported;
    uint previousSamples = uint( nrd.mJitter.z );
    uint samples = Parameters.mFrame.y;
    if ( cameraPath ) {
        uint cap = Parameters.mOptions.w;
        samples = min( samples, cap - min( previousSamples, cap ) );
    }

    // Accumulate diffuse and specular signals separately with their hit-distance statistics.
    // Unsupported camera paths accumulate the full-radiance fallback instead.
    float3 diffuse = 0;
    float3 specular = 0;
    float3 fallback = 0;
    float diffuseHit = 0;
    uint diffuseHitSamples = 0;
    float specularHit = NRD_FrontEnd_SpecHitDistAveraging_Begin();
    uint selectionSeed = Parameters.mFrame.x ^ 0x68bc21ebu;
    float selectionJitter = Random( selectionSeed );
    uint bayerIndex = ( pixel.x & 1u ) * 2u + ( ( pixel.y & 1u ) ^ ( pixel.x & 1u ) );
    float specularProbability = found ? SpecularProbability( s, -direction, ALL_LOBES ) : 1.0;
    if ( specularProbability > 0 && specularProbability < 1 ) {
        specularProbability = clamp( specularProbability, 0.25, 0.75 );
    }
    if ( !path ) {
        specularProbability = ( Parameters.mOptions.z & 2u ) == 0 ? 1.0 :
            ( Parameters.mOptions.z & 1u ) == 0 ? 0.0 : specularProbability;
    }
    for ( uint sample = 0; sample < samples; ++sample ) {
        float3 cameraOrigin = origin;
        float3 cameraDirection = direction;
        if ( cameraPath ) {
            random = PixelSampleSeed( pixel, sample );
            GenerateCameraRay( pixel, Random2D( random ), cameraOrigin, cameraDirection );
            float4 lensRight = Effects().mLensRight;
            if ( lensRight.w > 0 ) {
                ApplyDepthOfField( lensRight, Effects().mLensUp, Effects().mLensForward.xyz,
                    Random2D( random ), cameraOrigin, cameraDirection );
            }
        }

        // One probability-compensated path per sample, rather than two full
        // transport paths. Bayer + shared temporal rotation supplies both lobes
        // in a 3x3 neighborhood for NRD hit-distance reconstruction.
        float selector = frac( ( float( bayerIndex ) + 0.5 ) * 0.25 + selectionJitter + float( sample ) * 0.5 );
        bool isDiffuse = selector >= specularProbability;
        float selectionWeight = cameraPath ? 1.0 : rcp( isDiffuse ? 1.0 - specularProbability : specularProbability );
        if ( !cameraPath && ( !found || ( !path && ( Parameters.mOptions.z & ( isDiffuse ? 2u : 1u ) ) == 0 ) ) ) {
            continue;
        }

        float3 rayOrigin = cameraOrigin;
        float3 rayDirection = cameraDirection;
        float3 value = 0;
        float3 weight = 1;
        float distance = 0;
        uint depth = path ? Parameters.mFrame.z : ( isDiffuse ? Parameters.mFrame.z : Parameters.mOptions.w ) + 1u;
        PreviousVertex previous = ( PreviousVertex )0;
        bool trace = true;

        if ( !cameraPath ) {
            uint lobes = isDiffuse ? DIFFUSE : SPECULAR;
            value = SampleDirect( s, -direction, random, lobes, path );
            trace = depth > 0;
            if ( trace ) {
                BSDFSample bounce = SampleBSDF( s, -direction, random, lobes );
                trace = bounce.mPDF > 0 && any( bounce.mValue > 0 );
                if ( trace ) {
                    previous.mPosition = s.mPosition;
                    previous.mBSDFPDF = bounce.mDelta ? 0 : bounce.mPDF;
                    previous.mLightCount = LightCount( path );
                    rayOrigin = Offset( s, bounce.mDirection );
                    rayDirection = bounce.mDirection;
                    weight = bounce.mValue * abs( dot( s.mNormal, bounce.mDirection ) ) / bounce.mPDF;
                    --depth;
                }
            }
        }

        if ( trace ) {
            value += IntegrateWithHitDistance( rayOrigin, rayDirection, random, depth,
                         cameraPath, previous, distance ) *
                     weight;
        }

        if ( cameraPath ) {
            fallback += value;
        }
        else if ( isDiffuse ) {
            diffuse += value * selectionWeight;
            if ( distance > 0 ) {
                diffuseHit += distance;
                ++diffuseHitSamples;
            }
        }
        else {
            specular += value * selectionWeight;
            if ( distance > 0 ) {
                NRD_FrontEnd_SpecHitDistAveraging_Add( specularHit, distance );
            }
        }
    }

    if ( !supported ) {
        float3 sum = cameraPath ? fallback : diffuse + specular;
        float3 history = previousSamples > 0 ? RayTextures[Parameters.mHistory].Load( int3( pixel, 0 ) ).rgb : float3( 0 );
        float3 average = ( history * float( previousSamples ) + sum ) / float( max( previousSamples + samples, 1u ) );
        RayImages[Parameters.mAuxiliary][pixel] = float4( average, 1 );
        RayImages[Parameters.mOutput][pixel] = float4( path ? average : ( found && Parameters.mOptions.y != 0 ? HybridDirect( s, -direction, random ) : float3( 0 ) ), 1 );
        // Glass and media retain the unfiltered progressive reference.
        RayImages[nrd.mImages1.w][pixel] = float4( average, 0 );
        return;
    }

    diffuseHit /= max( diffuseHitSamples, 1u );
    NRD_FrontEnd_SpecHitDistAveraging_End( specularHit );
    diffuse /= max( samples, 1u );
    specular /= max( samples, 1u );
    float3 diffuseFactor;
    float3 specularFactor;
    // Remove the material factors expected by NRD before packing its signals. The same factors
    // are stored for reconstruction after denoising.
    NRD_MaterialFactors( s.mNormal, -direction, s.mBase * ( 1 - s.mMetallic ), s.mF0, s.mRoughness, diffuseFactor, specularFactor );
    float viewZ = mul( nrd.mWorldToView, float4( s.mPosition, 1 ) ).z;
    float4 packedDiffuse;
    float4 packedSpecular;
    // Use the packing required by the selected NRD method. REBLUR and RELAX differ in their
    // hit-distance/radiance signal encoding.
    if ( nrd.mMethod == 0 ) {
        packedDiffuse = REBLUR_FrontEnd_PackRadianceAndNormHitDist( diffuse / diffuseFactor,
            REBLUR_FrontEnd_GetNormHitDist( diffuseHit, viewZ, float3( 3, .1, 20 ), 1 ), true );
        packedSpecular = REBLUR_FrontEnd_PackRadianceAndNormHitDist( specular / specularFactor,
            REBLUR_FrontEnd_GetNormHitDist( specularHit, viewZ, float3( 3, .1, 20 ), s.mRoughness ), true );
    }
    else {
        packedDiffuse = RELAX_FrontEnd_PackRadianceAndHitDist( diffuse / diffuseFactor, diffuseHit, true );
        packedSpecular = RELAX_FrontEnd_PackRadianceAndHitDist( specular / specularFactor, specularHit, true );
    }
    RayImages[nrd.mImages0.x][pixel] = packedDiffuse;
    RayImages[nrd.mImages0.y][pixel] = packedSpecular;
    RayImages[nrd.mImages0.z][pixel] = NRD_FrontEnd_PackNormalAndRoughness( s.mNormal, s.mRoughness, 0 );
    RayImages[nrd.mImages0.w][pixel] = float4( viewZ, 0, 0, 0 );
    // A skipped diffuse lobe on a pure metal/black base must not receive a
    // neighboring dielectric's diffuse reconstruction through the epsilon factor.
    float3 diffuseResolveFactor = s.mMetallic >= 0.999 || !any( s.mBase > 0 ) ? float3( 0 ) : diffuseFactor;
    RayImages[nrd.mImages1.y][pixel] = float4( diffuseResolveFactor, 1 );
    RayImages[nrd.mImages1.z][pixel] = float4( specularFactor, 1 );
    float3 base = path ? s.mEmission : float3( 0 );
    RayImages[nrd.mImages1.w][pixel] = float4( base, 1 );
    // Keep a raw image for disabled/split comparisons; NRD never feeds its output
    // back into the tracer's progressive reference history.
    RayImages[Parameters.mAuxiliary][pixel] = float4( diffuse + specular + base, 1 );
    RayImages[Parameters.mOutput][pixel] = float4( path ? diffuse + specular + base : ( Parameters.mOptions.y != 0 ? HybridDirect( s, -direction, random ) : float3( 0 ) ), 1 );
}

#endif
