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

// Produce current-frame HDR and the material, depth and motion guides for DLSS RR.
// This shader owns its raygeneration implementation independently of the beauty pass.


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

// Copyright (c) 2023-2026 NVIDIA CORPORATION. All rights reserved.
// Copyright 2026 ケイト
// SPDX-License-Identifier: Apache-2.0
// Specular albedo and homogeneous reprojection adapted from:
// https://github.com/nvpro-samples/vk_gltf_renderer/blob/8c0ef8357b19acd05f5ae988a6c9cb912f39909b/shaders/dlss_util.h


struct DlssRayData {
    float4x4 mViewProjection;
    float4x4 mPreviousViewProjection;
    uint4 mImages0;
    uint4 mImages1;
    uint64_t mPreviousPositions;
    uint mWidth;
    uint mHeight;
    float4 mJitter;
    uint4 mGuideOptions;
};

// Environment BRDF approximation from Ray Tracing Gems, chapter 32, as used
// by NVIDIA's reference renderer. alpha is the squared perceptual roughness.
float3 GetDlssSpecularAlbedo( float3 color, float alpha, float normalView ) {
    normalView = abs( normalView );
    float4 x = float4( 1, normalView, normalView * normalView, normalView * normalView * normalView );
    float4 y = float4( 1, alpha, alpha * alpha, alpha * alpha * alpha );
    const float2x2 biasNumerator = float2x2( .99044, -1.28514, 1.29678, -.755907 );
    const float3x3 biasDenominator = float3x3( 1, 2.92338, 59.4188, 20.3225, -27.0302, 222.592, 121.563, 626.13, 316.627 );
    const float2x2 scaleNumerator = float2x2( .0365463, 3.32707, 9.0632, -9.04756 );
    const float3x3 scaleDenominator = float3x3( 1, 3.59685, -1.36772, 9.04401, -16.3174, 9.22949, 5.56589, 19.7886, -20.2123 );
    float bias = dot( mul( biasNumerator, x.xy ), y.xy ) / dot( mul( biasDenominator, x.xyw ), y.xyw );
    float scale = dot( mul( scaleNumerator, x.xy ), y.xy ) / dot( mul( scaleDenominator, x.xzw ), y.xyw );
    bias *= saturate( color.g * 50.0 );
    return max( color * max( scale, 0.0 ) + max( bias, 0.0 ), 0.0 );
}

float2 GetDlssMotion( DlssRayData data, float4 currentWorld, float4 previousWorld ) {
    float4 current = mul( data.mViewProjection, currentWorld );
    float4 previous = mul( data.mPreviousViewProjection, previousWorld );
    if ( data.mJitter.z == 0 || current.w <= 0 || previous.w <= 0 ) {
        return 0;
    }
    // GenerateCameraRay uses a top-left pixel origin and flips NDC Y.
    const float2 ndcToPixel = float2( .5, -.5 );
    return ( previous.xy / previous.w - current.xy / current.w ) * ndcToPixel * float2( data.mWidth, data.mHeight );
}

// Primary-surface replacement follows the clear specular transmission chain.
// The noisy transport stays unchanged; these virtual positions are only RR guides.
// Adapted from NVIDIA's Apache-2.0 clear-glass implementation:
// https://github.com/nvpro-samples/vk_gltf_renderer/blob/8c0ef8357b19acd05f5ae988a6c9cb912f39909b/shaders/pathtrace_functions.h.slang
static const uint kDlssMaxGlassLayers = 8;
static const float kDlssClearTransmission = .9;
static const float kDlssClearRoughness = .05;
static const float kDlssClearOpacity = .99;
static const float kDlssClearTransmittance = .9;

struct DlssGlassGuide {
    Surface mSurface;
    float3 mPosition;
    float3 mPreviousPosition;
};

float3 GetDlssPreviousPosition( DlssRayData data, Hit hit ) {
    uint3 indices = LoadTriangleIndices( hit.mTriangle );
    float3 barycentric = float3( 1.0 - hit.mBarycentrics.x - hit.mBarycentrics.y, hit.mBarycentrics );
    float3 a = LoadBufferValue<float4>( data.mPreviousPositions, indices.x * sizeof( float4 ) ).xyz;
    float3 b = LoadBufferValue<float4>( data.mPreviousPositions, indices.y * sizeof( float4 ) ).xyz;
    float3 c = LoadBufferValue<float4>( data.mPreviousPositions, indices.z * sizeof( float4 ) ).xyz;
    return a * barycentric.x + b * barycentric.y + c * barycentric.z;
}

bool IsDlssClearGlass( Surface surface ) {
    return surface.mModel == MATERIAL_DIELECTRIC &&
           surface.mTransmission > kDlssClearTransmission &&
           surface.mRoughness < kDlssClearRoughness &&
           surface.mAlpha >= kDlssClearOpacity &&
           !any( surface.mScattering > 0 );
}

bool GetDlssGlassGuide( DlssRayData data, Surface primary, float3 cameraDirection,
    inout uint random, out DlssGlassGuide guide ) {
    guide = ( DlssGlassGuide )0;
    if ( data.mGuideOptions.x == 0 || !IsDlssClearGlass( primary ) ) {
        return false;
    }

    Surface surface = primary;
    float3 direction = cameraDirection;
    float pathLength = 0;
    float3 transmittance = 1;
    MediumStack media = ( MediumStack )0;
    uint scatteringDepth = 0;

    for ( uint layer = 0; layer < kDlssMaxGlassLayers; ++layer ) {
        // Match transport's boundary bookkeeping and bounce budget. Otherwise a
        // dark, bounce-limited path would receive a bright albedo from behind it.
        bool nullBoundary = NullMediumBoundary( surface, media );
        if ( !nullBoundary && ++scatteringDepth >= Parameters.mFrame.z ) {
            return false;
        }

        if ( surface.mThinWalled ) {
            // A thin pane has no interior segment and leaves direction unchanged.
            transmittance *= exp( -surface.mAbsorption * surface.mThickness /
                max( abs( dot( surface.mNormal, direction ) ), 1e-6 ) );
        }
        else {
            uint outsideDepth = surface.mFrontFace ? media.mCount : ( media.mCount > 1 ? media.mCount - 1 : 0 );
            float relativeIOR = surface.mIOR / ReadMedium( media, outsideDepth ).mIOR;
            float eta = surface.mFrontFace ? relativeIOR : 1.0 / relativeIOR;
            float3 transmitted;
            if ( DielectricRefract( -direction, surface.mNormal, eta, transmitted ) ) {
                direction = transmitted;
                if ( !CrossMediumBoundary( media, surface ) ) {
                    return false;
                }
            }
            else {
                // Total internal reflection stays inside the current medium.
                direction = reflect( direction, surface.mNormal );
            }
        }

        Surface next;
        Hit hit;
        // Fixed alpha coverage keeps guide edges stable across stochastic frames.
        if ( !Intersect( Offset( surface, direction ), direction, RAY_MAX_DISTANCE,
                 random, next, hit, false, false, true ) ) {
            return false;
        }
        float segmentLength = length( next.mPosition - surface.mPosition );
        pathLength += segmentLength;
        transmittance *= exp( -ReadMedium( media, media.mCount ).mAbsorption * segmentLength );
        if ( min( transmittance.x, min( transmittance.y, transmittance.z ) ) < kDlssClearTransmittance ) {
            // Strong absorption/tint no longer describes a clear view of the rear surface.
            return false;
        }
        if ( IsDlssClearGlass( next ) ) {
            surface = next;
            continue;
        }
        if ( next.mModel == MATERIAL_MEDIUM ||
             ( next.mModel == MATERIAL_DIELECTRIC && any( next.mScattering > 0 ) ) ) {
            return false;
        }

        guide.mSurface = next;
        guide.mPosition = primary.mPosition + cameraDirection * pathLength;
        // Previous vertices include rigid motion, skinning, cloth and foliage.
        // Move the virtual point by the rear surface's actual deformation delta.
        guide.mPreviousPosition = guide.mPosition + GetDlssPreviousPosition( data, hit ) - next.mPosition;
        return true;
    }
    return false;
}

void RenderDlssInputs( uint2 pixel ) {
    DlssRayData data = LoadBufferValue<DlssRayData>( Parameters.mReconstruction, 0 );
    float3 origin;
    float3 direction;
    GenerateCameraRay( pixel, .5 + data.mJitter.xy, origin, direction );

    // A stable primary ray supplies the geometric guides. Alpha coverage uses
    // a fixed threshold, avoiding stochastic guide changes from frame to frame.
    uint guideSeed = pixel.x + pixel.y * data.mWidth;
    Surface surface;
    Hit hit;
    bool found = Intersect( origin, direction, RAY_MAX_DISTANCE, guideSeed, surface, hit, false, false, true );
    float3 diffuse = 0;
    float3 specular = 0;
    float4 normalRoughness = float4( 0, 0, 1, 1 );
    float depth = 1.0;
    float2 motion = 0;
    if ( found ) {
        diffuse = surface.mBase * ( 1.0 - surface.mMetallic ) * ( 1.0 - surface.mTransmission );
        specular = GetDlssSpecularAlbedo( surface.mF0, surface.mRoughness * surface.mRoughness, dot( surface.mNormal, -direction ) );
        normalRoughness = float4( surface.mNormal, surface.mRoughness );
        float3 position = surface.mPosition;
        float3 previous = GetDlssPreviousPosition( data, hit );
        DlssGlassGuide glass;
        if ( GetDlssGlassGuide( data, surface, direction, guideSeed, glass ) ) {
            float fresnel = DielectricFresnel( dot( surface.mNormal, -direction ), surface.mFrontFace ? surface.mIOR : 1.0 / surface.mIOR );
            diffuse = lerp( glass.mSurface.mBase, surface.mBase, fresnel );
            normalRoughness = float4( glass.mSurface.mNormal, glass.mSurface.mRoughness );
            position = glass.mPosition;
            previous = glass.mPreviousPosition;
        }
        float4 clip = mul( data.mViewProjection, float4( position, 1 ) );
        depth = saturate( clip.z / clip.w );
        motion = GetDlssMotion( data, float4( position, 1 ), float4( previous, 1 ) );
    }
    else if ( Parameters.mCamera.w == 0 ) {
        // A perspective sky is at infinity: w=0 cancels camera translation.
        // Parallel orthographic sky rays have no finite projected point.
        motion = GetDlssMotion( data, float4( direction, 0 ), float4( direction, 0 ) );
    }

    // RR consumes fresh radiance, including glass/media, rather than the running
    // beauty average or NRD's demodulated lobes. It owns its temporal history.
    uint samples = max( Parameters.mFrame.y, 1u );
    float3 color = 0;
    for ( uint sample = 0; sample < samples; ++sample ) {
        uint random = PixelSampleSeed( pixel, sample );
        float2 offset = .5 + data.mJitter.xy;
        if ( samples > 1 ) {
            offset += Random2D( random ) - .5;
        }
        float3 cameraOrigin;
        float3 cameraDirection;
        GenerateCameraRay( pixel, offset, cameraOrigin, cameraDirection );
        float4 lensRight = Effects().mLensRight;
        if ( lensRight.w > 0 ) {
            ApplyDepthOfField( lensRight, Effects().mLensUp, Effects().mLensForward.xyz,
                Random2D( random ), cameraOrigin, cameraDirection );
        }
        color += Integrate( cameraOrigin, cameraDirection, random, Parameters.mFrame.z, true, ( PreviousVertex )0 );
    }
    RayImages[data.mImages0.x][pixel] = float4( color / float( samples ), 1 );
    RayImages[data.mImages0.y][pixel] = float4( diffuse, 1 );
    RayImages[data.mImages0.z][pixel] = float4( specular, 1 );
    RayImages[data.mImages0.w][pixel] = normalRoughness;
    RayImages[data.mImages1.x][pixel] = float4( depth, 0, 0, 0 );
    RayImages[data.mImages1.y][pixel] = float4( motion, 0, 0 );
}

[shader( "raygeneration" )]
void main() {
    uint2 pixel = DispatchRaysIndex().xy;
    RenderDlssInputs( pixel );
}
