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

// Dispatch the requested path-traced or hybrid lighting mode for each camera pixel. History
// handling and NRD signal generation branch here before tracing work is submitted.

#include "base/HeightMapping_Helpers.slang"
#include "base/Memory_Helpers.slang"
#include "base/RayTracing_Bindings.slang"
#include "PathTracing_Common.slang"

// Keep transport helpers eligible for inlining. Large out-parameter records
// made the forced-noinline path measurably slower on the GPU.
// https://developer.nvidia.com/blog/best-practices-for-using-nvidia-rtx-ray-tracing-updated/
#include "base/PathTracing_AmbientOcclusion.slang"
#include "base/PathTracing_Bsdf.slang"
#include "base/PathTracing_Camera.slang"
#include "base/PathTracing_Environment.slang"
#include "base/PathTracing_Integrator.slang"
#include "base/PathTracing_Lights.slang"
#include "base/PathTracing_Media.slang"
#include "base/PathTracing_Sampling.slang"
#include "base/PathTracing_Surface.slang"

// Transport and sampling are adapted from PBRT v4, in RGB for Mikoto's glTF materials.
// PBRT-derived portions: Copyright (c) 1998-2020 Matt Pharr, Wenzel Jakob, Greg Humphreys.
// https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/cpu/integrators.cpp

// Use the frame graph's shared SRV, UAV and sampler bindings.
MKT_DECLARE_TEXTURE_SRV( Texture2D<float4> )
RayTextures;
MKT_DECLARE_TEXTURE_UAV( RWTexture2D<float4> )
RayImages;
MKT_DECLARE_SAMPLERS RaySamplers;

// Different rays in a wave can hit different materials.
Texture2D<float4> MaterialMap( uint index ) {
    return RayTextures[NonUniformResourceIndex( index )];
}

SamplerState MaterialSampler() {
    return RaySamplers[Parameters.mMaterialSampler];
}

Texture2D<float4> AtmosphereMap( uint index ) {
    return RayTextures[NonUniformResourceIndex( Parameters.mAtmosphereTextures[index] )];
}

SamplerState GetAtmosphereSampler() {
    return RaySamplers[Parameters.mAtmosphereSampler];
}

#define MKT_ATMOSPHERE_RAY_TRACING
#include "Atmosphere_Common.slang"

struct ViewEffectsRecord {
    AtmosphereData mAtmosphere;
    float4 mControl;
    float4 mBackground;
    float4 mLensRight;
    float4 mLensUp;
    float4 mLensForward;
};

ViewEffectsRecord Effects() {
    return LoadBufferValue<ViewEffectsRecord>( Parameters.mViewEffects, 0 );
}

uint PixelSampleSeed( uint2 pixel, uint sample ) {
    // Reserve disjoint sample blocks even when samples per frame changes.
    // Camera edits clear accumulation while this independent stream continues.
    // https://pbr-book.org/4ed/Sampling_and_Reconstruction/Independent_Sampler
    uint sampleIndex = Parameters.mFrame.x * SAMPLE_STREAMS_PER_FRAME + sample;
    return ( pixel.x + pixel.y * DispatchRaysDimensions().x + 1u ) ^ ( sampleIndex * SAMPLE_SEED_STRIDE );
}

void RenderPathTracingWithHistory( uint2 pixel, uint previous ) {
    uint sampleCap = Parameters.mOptions.w;
    uint samples = min( Parameters.mFrame.y, sampleCap - min( previous, sampleCap ) );
    float3 sum = float3( 0.0 );

    for ( uint sample = 0; sample < samples; ++sample ) {
        uint random = PixelSampleSeed( pixel, sample );
        float2 jitter = Random2D( random );
        float3 origin;
        float3 direction;
        GenerateCameraRay( pixel, jitter, origin, direction );

        float4 lensRight = Effects().mLensRight;
        if ( lensRight.w > 0.0 ) {
            float4 lensUp = Effects().mLensUp;
            float3 lensForward = Effects().mLensForward.xyz;
            float2 lensSample = Random2D( random );
            ApplyDepthOfField( lensRight, lensUp, lensForward, lensSample, origin, direction );
        }

        sum += Integrate( origin, direction, random, Parameters.mFrame.z, true, ( PreviousVertex )0 );
    }

    float3 history = previous > 0 ? RayTextures[Parameters.mHistory].Load( int3( pixel, 0 ) ).rgb : float3( 0.0 );
    float3 average = ( history * float( previous ) + sum ) / float( max( previous + samples, 1u ) );

    // Both destinations receive scene-linear radiance. The display passes apply
    // bloom, tone mapping and overlays after accumulation.
    RayImages[Parameters.mAuxiliary][pixel] = float4( average, 1.0 );
    RayImages[Parameters.mOutput][pixel] = float4( average, 1.0 );
}

void RenderPathTracing( uint2 pixel ) {
    RenderPathTracingWithHistory( pixel, Parameters.mOptions.x );
}

// Options.y carries the enabled hybrid shadow sample count (zero disables it).
// Average the full direct-light estimator so light selection and finite emitter
// sampling both retain their original energy as the sample budget increases.
float3 HybridDirect( Surface surface, float3 toEye, inout uint random ) {
    uint samples = min( Parameters.mOptions.y, 16u );
    float3 radiance = 0;
    for ( uint sample = 0; sample < samples; ++sample ) {
        radiance += Direct( surface, toEye, random, true );
    }
    return radiance / float( max( samples, 1u ) );
}

void RenderHybridLighting( uint2 pixel ) {
    static const uint reflectionOption = 1u;
    static const uint giOption = 2u;
    bool gi = ( Parameters.mOptions.z & giOption ) != 0;
    bool reflections = ( Parameters.mOptions.z & reflectionOption ) != 0;
    uint indirectSamples = Parameters.mFrame.y;

    uint random = PixelSampleSeed( pixel, 0 );
    float3 origin;
    float3 direction;
    GenerateCameraRay( pixel, float2( 0.5 ), origin, direction );

    Surface surface;
    Hit hit;
    float3 direct = float3( 0.0 );
    float3 indirect = float3( 0.0 );

    // Only opaque raster surfaces consume the hybrid lighting outputs.
    if ( Intersect( origin, direction, RAY_MAX_DISTANCE, random, surface, hit, true ) ) {
        if ( Parameters.mOptions.y != 0 ) {
            direct = HybridDirect( surface, -direction, random );
        }

        for ( uint sample = 0; sample < indirectSamples; ++sample ) {
            if ( gi ) {
                indirect += Indirect( surface, -direction, random, DIFFUSE, Parameters.mFrame.z );
            }

            if ( reflections ) {
                indirect += Indirect( surface, -direction, random, SPECULAR, Parameters.mOptions.w );
            }
        }
    }

    if ( gi || reflections ) {
        uint previous = Parameters.mOptions.x;
        float3 history = previous > 0 ? RayTextures[Parameters.mHistory].Load( int3( pixel, 0 ) ).rgb : float3( 0.0 );
        indirect = ( history * float( previous ) + indirect ) / float( previous + indirectSamples );
    }
    else {
        indirect /= float( indirectSamples );
    }

    RayImages[Parameters.mAuxiliary][pixel] = float4( indirect, 1.0 );
    RayImages[Parameters.mOutput][pixel] = float4( direct, 1.0 );
}

#if MKT_NRD_AVAILABLE && MKT_NRD_TRACE
#include "base/Denoiser_Helpers.slang"
#endif

[shader( "raygeneration" )]
void main() {
    uint2 pixel = DispatchRaysIndex().xy;
    RenderAmbientOcclusion( pixel );

#if MKT_NRD_AVAILABLE && MKT_NRD_TRACE
    // This variant always has a denoiser data buffer. Its entry path owns the
    // progressive fallback too, avoiding a second copy of the full transport loop.
    RenderDenoiserInputs( pixel );
#else
    if ( Parameters.mFrame.w != 0 ) {
        RenderPathTracing( pixel );
    }
    else {
        RenderHybridLighting( pixel );
    }
#endif
}
