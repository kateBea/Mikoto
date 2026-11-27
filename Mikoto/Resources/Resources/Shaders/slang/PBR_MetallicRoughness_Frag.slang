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

// Further ref: https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/glTF-WebGL-PBR/shaders/pbr-frag.glsl
// https://github.com/KhronosGroup/glTF-Sample-Viewer.git (branch glTF-WebGL-PBR)

// Forward surface lighting: evaluate material maps, direct lights, shadow visibility and image-
// based lighting. The final radiance also accounts for the selected surface model, transmission
// and participating-media composition.

#pragma once

#include "base/AreaLighting_Helpers.slang"
#include "base/Atmosphere_Helpers.slang"
#include "base/Base.slang"
#include "base/Camera_Helpers.slang"
#include "base/Clouds_Helpers.slang"
#include "base/ClusteringLights_Helpers.slang"
#include "base/Fog_Helpers.slang"
#include "base/HeightMapping_Helpers.slang"
#include "base/Lighting_Helpers.slang"
#include "base/Material_Helpers.slang"
#include "base/Math_Helpers.slang"
#include "base/Memory_Helpers.slang"
#include "base/MetallicRoughnessBRDF.slang"
#include "base/ShadowMap_Helpers.slang"
#include "base/SurfaceShading_Helpers.slang"
#include "base/Tonemap_Helpers.slang"
#include "base/Volume_Helpers.slang"

struct DrawParams {
    Pointer mGeometryDescPtr;
    Pointer mSkinningPtr;
    Pointer mGeometryPtr;
    Pointer mCameraPtr;

    Pointer mMaterialsPtr;

    Pointer mClusterPtr;
    Pointer mLightsPtr;

    float2 mGridSize1;
    float mGridSizeZ;
    uint mRayIndirect;

    uint mClusterBufferSize;
    float mSsaoIntensity;

    uint mSsaoTextureID;

    uint mBrdfColorTargetID;
    uint mPrefilterCubeRtID;
    uint mIrradianceCubeRtID;

    uint mSamplerCubeID;
    uint mSamplerBasicID;

    uint mActiveLightCount;

    uint mPrefilterMipLevelsAndSkybox;
    float mScaleIblAmbient;

    uint mRayDirect;

    uint mShadowInfoBufferID;
    uint mShadowRecordCount;
};

MKT_DECLARE_STRUCTURED_SRV buffers;

MKT_DECLARE_TEXTURE_SRV( Texture2D )
textures2D;
MKT_DECLARE_TEXTURE_SRV( TextureCube )
texturesCube;

MKT_DECLARE_SAMPLERS samplers;

MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );

struct BaseInputs {
    float3 mCoatNormal;
    uint mModel;
    float4 mSettings;
    float mBands;
    float mPerceptualRoughness;
    float mMetallic;
    float3 mSpecularEnvironmentR0;
    float3 mSpecularEnvironmentR90;
    float mAlphaRoughness;
    float3 mDiffuseColor;
    float3 mSpecularColor;
};

struct FSInput {
    float3 mFragmentViewPos : TEXCOORD0;
    float3 mFragmentWorldPos : TEXCOORD1;
    float3 mNormal : NORMAL;
    float2 mTexCoord0 : TEXCOORD3;
    float2 mTexCoord1 : TEXCOORD4;
    float4 mVertexColor : TEXCOORD5;

    nointerpolation uint mMaterialIndex : TEXCOORD6;
    nointerpolation uint mFaceFlags : TEXCOORD7;
    nointerpolation uint mDrawIdentity : TEXCOORD8;

    float4 mScreenPos : SV_Position;
    bool mIsFrontFace : SV_IsFrontFace;
};

// Calculation of the lighting contribution from an optional Image Based Light source.
// Use the split-sum environment approximation: diffuse irradiance plus roughness-prefiltered
// specular radiance. Ray-lit lobes are omitted here so hybrid lighting does not add them twice.
float3 GetIBLContribution( float NdotV, float3 diffuseColor, float3 specularColor, float perceptualRoughness, float3 n,
    float3 reflection, Texture2D brdfLut, TextureCube irradianceMap, TextureCube prefilterMap,
    SamplerState sampler, bool useRayLighting, out float3 environmentSpecular ) {
    // Avoid rough reflections sample invalid mip levels
    float lod = perceptualRoughness * ( ( parameters.mPrefilterMipLevelsAndSkybox & 0x07ffffffu ) - 1 );

    // retrieve a scale and bias to F0. See [1], Figure 3
    float3 brdf = brdfLut.SampleLevel( sampler, float2( NdotV, 1.0 - perceptualRoughness ), 0 ).rgb;

    // Convolved environment maps already contain linear radiance. Add it to
    // direct lighting before the final exposure/tone-map pass; tone-mapping and
    // decoding it here crushes ambient light and applies exposure twice.
    float3 diffuseLight = irradianceMap.SampleLevel( sampler, n, 0 ).rgb;
    float3 specularLight = prefilterMap.SampleLevel( sampler, reflection, lod ).rgb;

    float3 diffuse = diffuseLight * diffuseColor;
    float3 specular = specularLight * ( specularColor * brdf.x + brdf.y );

    diffuse *= parameters.mScaleIblAmbient;
    specular *= parameters.mScaleIblAmbient;

    // Ray-traced diffuse already includes visibility and sky illumination.
    // Replace only the enabled lobe to avoid counting environment light twice.
    bool rayDiffuse = useRayLighting && ( parameters.mPrefilterMipLevelsAndSkybox & 0x40000000u ) != 0;
    bool raySpecular = useRayLighting && ( parameters.mPrefilterMipLevelsAndSkybox & 0x20000000u ) != 0;
    environmentSpecular = raySpecular ? float3( 0.0 ) : specular;
    return ( rayDiffuse ? float3( 0.0 ) : diffuse ) + environmentSpecular;
}

float3 ComputeDirectionalLightContribution( float3 n, float3 v, LightInfo lightInfo, BaseInputs inputs, float visibility = 1.0 ) {
    float3 l = normalize(
        -lightInfo.mDirection
            .xyz );               // Vector from surface point to light (multiply by -1 because otherwise wrong face is lit)
    float3 h = normalize( l + v );// Half vector between both l and v

    float NdotL = clamp( dot( n, l ), 0.001, 1.0 );
    float NdotV = clamp( abs( dot( n, v ) ), 0.001, 1.0 );
    float NdotH = clamp( dot( n, h ), 0.0, 1.0 );
    float LdotH = clamp( dot( l, h ), 0.0, 1.0 );
    float VdotH = clamp( dot( v, h ), 0.0, 1.0 );

    PBRInfo pbrInputs = PBRInfo( NdotL, NdotV, NdotH, LdotH, VdotH, inputs.mPerceptualRoughness, inputs.mMetallic,
        inputs.mSpecularEnvironmentR0, inputs.mSpecularEnvironmentR90, inputs.mAlphaRoughness,
        inputs.mDiffuseColor, inputs.mSpecularColor );

    // Calculate the shading terms for the microfacet specular shading model
    float3 F = SpecularReflection( pbrInputs );// kS
    float G = GeometricOcclusion( pbrInputs );
    float D = MicrofacetDistribution( pbrInputs );

    // Calculation of analytical lighting contribution, ks + kD = 1 with kD = 1.0 - F
    float3 diffuseContrib = ( 1.0 - F ) * DiffuseBurley( pbrInputs );
    float3 specContrib = F * G * D / ( 4.0 * NdotL * NdotV );

    // Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
    float3 color = lightInfo.mDiffuse.xyz * SurfaceDirect(
                                               NdotL * ( diffuseContrib + specContrib ),
                                               inputs.mDiffuseColor, inputs.mSpecularColor, inputs.mPerceptualRoughness,
                                               n, v, l, inputs.mModel, inputs.mSettings, inputs.mBands, visibility, inputs.mCoatNormal );

    return color;
}

// Ref: Moving frostbite to PBR
// https://www.ea.com/frostbite/news/moving-frostbite-to-pb
// Window the inverse-square light response to zero at the authored range. Squaring the smooth
// window removes the hard cutoff at the light boundary.
float SmoothDistanceAtt( float d2, float invR2 ) {
    float howFar = d2 * invR2;
    float dontDie = saturate( 1.0 - howFar * howFar );
    return dontDie * dontDie;
}

// Combine inverse-square falloff with the finite-range window; the minimum squared distance
// bounds the response near the light origin.
float GetDistanceAtt( float3 L_unorm, float invR2 ) {
    float d2 = dot( L_unorm, L_unorm );
    float invSq = 1.0 / max( d2, 0.0001 );

    float fade = SmoothDistanceAtt( d2, invR2 );

    return invSq * fade;
}

// Map the spotlight direction cosine into its inner/outer cone window. Squaring the result
// softens the cone boundary.
float GetAngleAtt( float3 L, float3 lightDir, float scale, float offset ) {
    float howInsideCone = dot( lightDir, L );// are we in the cone or nah

    float cone = saturate( howInsideCone * scale + offset );
    cone *= cone;

    return cone;
}

float3 ComputePointlLightContribution( float3 n, float3 v, LightInfo lightInfo, BaseInputs inputs, float3 worldPos, float visibility = 1.0 ) {
    // Light vector and distance (v, l)
    const float3 lightVector = lightInfo.mPosition.xyz - worldPos;
    const float distance = length( lightVector );

    float attenuation = 1;

    const float radius = lightInfo.mRadius;
    const float lightInvSqrAttRadius = 1 / ( radius * radius );

    attenuation *= GetDistanceAtt( lightVector, lightInvSqrAttRadius );

    const float intensityBaseFactor = 1.0f;
    const float3 lightIntensity = lightInfo.mDiffuse.xyz * ( lightInfo.mIntensity * intensityBaseFactor ) * attenuation;

    const float3 l = lightVector / max( distance, 1e-5 );
    const float3 h = normalize( l + v );

    const float NdotL = clamp( dot( n, l ), 0.001, 1.0 );
    const float NdotV = clamp( abs( dot( n, v ) ), 0.001, 1.0 );
    const float NdotH = clamp( dot( n, h ), 0.0, 1.0 );
    const float LdotH = clamp( dot( l, h ), 0.0, 1.0 );
    const float VdotH = clamp( dot( v, h ), 0.0, 1.0 );

    PBRInfo pbrInputs = PBRInfo( NdotL, NdotV, NdotH, LdotH, VdotH, inputs.mPerceptualRoughness, inputs.mMetallic,
        inputs.mSpecularEnvironmentR0, inputs.mSpecularEnvironmentR90, inputs.mAlphaRoughness,
        inputs.mDiffuseColor, inputs.mSpecularColor );

    // Calculate the shading terms for the microfacet specular shading model
    const float3 F = SpecularReflection( pbrInputs );// kS
    const float G = GeometricOcclusion( pbrInputs );
    const float D = MicrofacetDistribution( pbrInputs );

    // Calculation of analytical lighting contribution, ks + kD = 1 with kD = 1.0 - F
    const float3 diffuseContrib = ( 1.0 - F ) * DiffuseBurley( pbrInputs );
    const float3 specContrib = F * G * D / ( 4.0 * NdotL * NdotV );

    // Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
    const float3 color = lightIntensity * SurfaceDirect(
                                              NdotL * ( diffuseContrib + specContrib ),
                                              inputs.mDiffuseColor, inputs.mSpecularColor, inputs.mPerceptualRoughness,
                                              n, v, l, inputs.mModel, inputs.mSettings, inputs.mBands, visibility, inputs.mCoatNormal );

    return color;
}

float3 ComputeSpotLightContribution( float3 n, float3 v, LightInfo lightInfo, BaseInputs inputs, float3 worldPos, float visibility = 1.0 ) {
    float3 lightVector = lightInfo.mPosition.xyz - worldPos;

    float distance = length( lightVector );
    float3 l = lightVector / max( distance, 1e-5 );

    // distance attenuation (Smooth fallof not physically correct)
    float distanceAttenuation = saturate( 1.0 - distance / lightInfo.mRadius );
    distanceAttenuation *= distanceAttenuation;

    // spotlight cone
    float theta = dot( normalize( -lightInfo.mDirection.xyz ), l );

    float epsilon = lightInfo.mCutOff - lightInfo.mOuterCutOff;
    float coneAttenuation = epsilon > 1e-6 ? saturate( ( theta - lightInfo.mOuterCutOff ) / epsilon )
                                           : ( theta >= lightInfo.mOuterCutOff ? 1.0 : 0.0 );

    // final attenuation
    float attenuation = distanceAttenuation * coneAttenuation;

    const float intensityBaseFactor = 1000.0f;
    const float3 lightIntensity = lightInfo.mDiffuse.xyz * ( lightInfo.mIntensity * intensityBaseFactor ) * attenuation;

    float3 h = normalize( l + v );

    float NdotL = clamp( dot( n, l ), 0.001, 1.0 );
    float NdotV = clamp( abs( dot( n, v ) ), 0.001, 1.0 );
    float NdotH = clamp( dot( n, h ), 0.0, 1.0 );
    float LdotH = clamp( dot( l, h ), 0.0, 1.0 );
    float VdotH = clamp( dot( v, h ), 0.0, 1.0 );

    PBRInfo pbrInputs = PBRInfo( NdotL, NdotV, NdotH, LdotH, VdotH, inputs.mPerceptualRoughness, inputs.mMetallic,
        inputs.mSpecularEnvironmentR0, inputs.mSpecularEnvironmentR90, inputs.mAlphaRoughness,
        inputs.mDiffuseColor, inputs.mSpecularColor );

    // Calculate the shading terms for the microfacet specular shading model
    float3 F = SpecularReflection( pbrInputs );// kS
    float G = GeometricOcclusion( pbrInputs );
    float D = MicrofacetDistribution( pbrInputs );

    // Calculation of analytical lighting contribution, ks + kD = 1 with kD = 1.0 - F
    float3 diffuseContrib = ( 1.0 - F ) * DiffuseBurley( pbrInputs );
    float3 specContrib = F * G * D / ( 4.0 * NdotL * NdotV );

    // Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
    float3 color = lightIntensity * SurfaceDirect(
                                        NdotL * ( diffuseContrib + specContrib ),
                                        inputs.mDiffuseColor, inputs.mSpecularColor, inputs.mPerceptualRoughness,
                                        n, v, l, inputs.mModel, inputs.mSettings, inputs.mBands, visibility, inputs.mCoatNormal );

    return color;
}

struct FSOutput {
    float4 mColor : SV_Target0;
// Export the surface response and existing environment-specular fallback for SSR. The
// reflection compositor can then replace the matching lobe without duplicating its energy.
#if defined( MKT_SSR_CAPTURE )
    float4 mSurfaceResponse : SV_Target1;
    float4 mEnvironmentSpecular : SV_Target2;
#endif
};

// Read the small record tail before selecting a face matrix. Keeping the six matrices out of
// the temporary metadata avoids expensive dynamically indexed local arrays.
ShadowMetadata LoadShadowMetadata( uint recordIndex ) {
    return buffers[parameters.mShadowInfoBufferID].LoadByteOffset<ShadowMetadata>(
        recordIndex * sizeof( ShadowRecord ) + ShadowMetadataOffset );
}

// Address one matrix within the CPU-mirrored ShadowRecord. The face index is a cascade for
// directional lights and a cube face for point lights.
float4x4 LoadShadowMatrix( uint recordIndex, uint face ) {
    return buffers[parameters.mShadowInfoBufferID].LoadByteOffset<float4x4>( recordIndex * sizeof( ShadowRecord ) +
                                                                             face * sizeof( float4x4 ) );
}

// Compare the stored blocker with the receiver plane at this exact texel center. Evaluating the
// geometric receiver plane reduces false self-shadowing on sloped surfaces.
float CompareShadowTexel( ShadowMetadata record, float4x4 viewProjection, Texture2D texture, int2 dimensions,
    int2 pixel, float3 ndc, float3 receiverWorld, float3 receiverNormal, float bias ) {
    if ( any( pixel < 0 ) || any( pixel >= dimensions ) ) {
        return 1;
    }
    float2 texelUV = ( float2( pixel ) + 0.5 ) / float2( dimensions );
    float storedDepth = texture.Load( int3( pixel, 0 ) ).r;
    float receiverDepth;
    if ( record.mLightType == LIGHT_TYPE_DIRECTIONAL ) {
        receiverDepth = DirectionalShadowReceiverDepth( viewProjection, texelUV, ndc, receiverNormal );
    }
    else {
        receiverDepth = LocalShadowReceiverDepth( viewProjection, texelUV, receiverWorld, receiverNormal,
            record.mPositionRange );
    }
    if ( receiverDepth < 0.0 || receiverDepth >= 1.0 ) {
        return 1.0;
    }
    return receiverDepth - bias <= storedDepth ? 1.0 : 0.0;
}

// Project a world-space sample into one light face and perform a receiver-aware depth
// comparison. Samples outside the map contribute unoccluded visibility.
float ShadowTexel( uint recordIndex, ShadowMetadata record, uint face, float3 sampleWorld, float3 receiverWorld,
    float3 receiverNormal, float bias ) {
    float4x4 viewProjection = LoadShadowMatrix( recordIndex, face );
    float4 clip = mul( viewProjection, float4( sampleWorld, 1 ) );
    if ( clip.w <= 0 ) {
        return 1;
    }
    float3 ndc = clip.xyz / clip.w;
    if ( ndc.z < 0 || ndc.z > 1 || any( abs( ndc.xy ) > 1 ) ) {
        return 1;
    }
    float2 uv = float2( ndc.x * .5 + .5, .5 - ndc.y * .5 );
    Texture2D texture = textures2D[NonUniformResourceIndex( record.TextureID( face ) )];
    uint width;
    uint height;
    texture.GetDimensions( width, height );
    int2 pixel = int2( uv * float2( width, height ) );
    return CompareShadowTexel( record, viewProjection, texture, int2( width, height ), pixel, ndc, receiverWorld,
        receiverNormal, bias );
}

// Percentage-closer soft shadows: blocker search, geometric penumbra, filtering.
// https://developer.download.nvidia.com/assets/gamedev/docs/PCSS_Integration.pdf
// Distribute shadow-filter taps over a disk using the golden angle. The square-root radial
// mapping gives approximately uniform sample density over disk area.
// Blocker search and soft filtering share one disk; fallback PCF uses a 3x3 neighborhood.
static const uint kShadowDiskSampleCount = 24;
static const uint kShadowPcfTapCount = 9;
static const float kShadowGoldenAngle = 2.39996323;

float2 ShadowDisk( uint index ) {
    float angle = float( index ) * kShadowGoldenAngle;
    return float2( cos( angle ), sin( angle ) ) * sqrt( ( float( index ) + 0.5 ) / float( kShadowDiskSampleCount ) );
}

// Search for blockers and estimate filter radius from blocker/receiver separation and light
// size. A negative result means the search found no blocking samples.
float ShadowPenumbra( ShadowMetadata record, float4x4 matrix, Texture2D texture,
    uint2 dimensions, float2 uv, float3 ndc, float3 world, float3 normal, float bias ) {
    if ( record.mBias.w == 0 || record.mBias.z <= 0 ) {
        return 0;
    }
    bool distant = record.mLightType == LIGHT_TYPE_DIRECTIONAL;
    float receiver = distant ? ndc.z : LocalShadowDepth( world, record.mPositionRange );
    // Directional world units -> UV and normalized depth; point source angular size.
    float uvScale = distant ? 0.5 * length( matrix[0].xyz ) : 0.5;
    float search = distant ? record.mBias.z / max( length( matrix[2].xyz ), 1e-6 ) * uvScale : record.mBias.z / max( length( world - record.mPositionRange.xyz ), 0.001 ) * uvScale;
    search = min( search, 0.1 );
    float blockers = 0;
    float sum = 0;
    for ( uint i = 0; i < kShadowDiskSampleCount; ++i ) {
        int2 pixel = int2( ( uv + ShadowDisk( i ) * search ) * float2( dimensions ) );
        if ( any( pixel < 0 ) || any( pixel >= int2( dimensions ) ) ) {
            continue;
        }
        float stored = texture.Load( int3( pixel, 0 ) ).r;
        float2 tapUV = ( float2( pixel ) + 0.5 ) / float2( dimensions );
        float plane = distant ? DirectionalShadowReceiverDepth( matrix, tapUV, ndc, normal ) : LocalShadowReceiverDepth( matrix, tapUV, world, normal, record.mPositionRange );
        if ( plane > 0 && stored < plane - bias ) {
            sum += stored;
            blockers += 1;
        }
    }
    if ( blockers == 0 ) {
        return -1;
    }
    float blocker = sum / blockers;
    float radius = distant ? max( receiver - blocker, 0 ) / max( length( matrix[2].xyz ), 1e-6 ) * record.mBias.z * uvScale : record.mBias.z * max( receiver - blocker, 0 ) / max( blocker, 0.0001 ) / max( length( world - record.mPositionRange.xyz ), 0.001 ) * uvScale;
    return min( radius, 0.1 );
}

// Average nearby depth comparisons instead of filtering raw depth. The filter stays tied to
// receiver geometry so map bias and sloping surfaces use compatible depths.
float ShadowPCF( uint recordIndex, ShadowMetadata record, uint face, float3 world, float3 normal, float bias ) {
    // All nine taps use one face and receiver projection. Keep matrix loads,
    // descriptor lookup, dimensions and the clip test outside the filter loop.
    float4x4 viewProjection = LoadShadowMatrix( recordIndex, face );
    // Orthographic row 2 maps world-space distances along the light's
    // depth axis to normalized depth. Keep contact bias constant across
    // cascades even when their caster padding/depth spans differ.
    if ( record.mLightType == LIGHT_TYPE_DIRECTIONAL ) {
        bias *= length( viewProjection[2].xyz );
    }
    float4 clip = mul( viewProjection, float4( world, 1 ) );
    if ( clip.w <= 0 ) {
        return 1;
    }
    float3 ndc = clip.xyz / clip.w;
    if ( ndc.z < 0 || ndc.z > 1 || any( abs( ndc.xy ) > 1 ) ) {
        return 1;
    }
    Texture2D texture = textures2D[NonUniformResourceIndex( record.TextureID( face ) )];
    uint width;
    uint height;
    texture.GetDimensions( width, height );
    float2 uv = float2( ndc.x * .5 + .5, .5 - ndc.y * .5 );
    int2 pixel = int2( uv * float2( width, height ) );
    float penumbra = ShadowPenumbra( record, viewProjection, texture, uint2( width, height ), uv, ndc, world, normal, bias );
    if ( penumbra < 0 ) {
        return 1;
    }
    if ( penumbra > 0 ) {
        float visibility = 0;
        for ( uint i = 0; i < kShadowDiskSampleCount; ++i ) {
            int2 tap = int2( ( uv + ShadowDisk( i ) * penumbra ) * float2( width, height ) );
            visibility += CompareShadowTexel( record, viewProjection, texture, int2( width, height ), tap, ndc, world, normal, bias );
        }
        return visibility / float( kShadowDiskSampleCount );
    }
    float sum = 0;
    for ( int y = -kShadowPcfRadius; y <= kShadowPcfRadius; ++y ) {
        for ( int x = -kShadowPcfRadius; x <= kShadowPcfRadius; ++x ) {
            sum += CompareShadowTexel( record, viewProjection, texture, int2( width, height ), pixel + int2( x, y ),
                ndc, world, normal, bias );
        }
    }
    return sum / float( kShadowPcfTapCount );
}

// Select the directional cascade or local-light face and evaluate visibility. Cached
// directional visibility is valid only when its camera-depth receiver matches the shaded
// fragment.
float LightShadow( LightInfo light, float3 world, float3 normal, float viewDepth, float3 screenPosition ) {
    if ( light.mShadowIndex < 0 || uint( light.mShadowIndex ) >= parameters.mShadowRecordCount ) {
        return 1;
    }
    uint recordIndex = uint( light.mShadowIndex );
    ShadowMetadata record = LoadShadowMetadata( recordIndex );
    if ( record.mFaceCount == 0 || record.mLightType != light.mActiveLightType ) {
        return 1;
    }
    float3 toLight = light.mActiveLightType == LIGHT_TYPE_DIRECTIONAL ? normalize( -light.mDirection.xyz )
                                                                     : normalize( light.mPosition.xyz - world );
    float bias = record.mBias.x + record.mBias.y * ( 1 - saturate( abs( dot( normal, toLight ) ) ) );
    if ( light.mActiveLightType == LIGHT_TYPE_DIRECTIONAL ) {
        float contact = record.TextureID( kDirectionalReceiverSlot ) == 0xffffffffu ? 1 :
            GrassContactVisibility( textures2D[NonUniformResourceIndex( record.TextureID( kDirectionalReceiverSlot ) )], screenPosition, false );
        // Tardif's filtered visibility applies only to the camera-depth receiver.
        // Transparent layers and uncovered MSAA samples retain per-surface shadow comparisons.
        // https://alextardif.com/shadowmapping.html
        if ( record.TextureID( kDirectionalVisibilitySlot ) != 0xffffffffu && record.TextureID( kDirectionalReceiverSlot ) != 0xffffffffu ) {
            int2 pixel = int2( screenPosition.xy );
            float depth = textures2D[NonUniformResourceIndex( record.TextureID( kDirectionalReceiverSlot ) )].Load( int3( pixel, 0 ) ).r;
            if ( abs( depth - screenPosition.z ) <= 2e-6 ) {
                return min( contact, textures2D[NonUniformResourceIndex( record.TextureID( kDirectionalVisibilitySlot ) )].Load( int3( pixel, 0 ) ).r );
            }
        }
        if ( viewDepth <= 0 || viewDepth > record.mCascadeSplits.w ) {
            return 1;
        }
        uint cascade = 0;
        while ( cascade + 1 < record.mFaceCount && viewDepth > record.mCascadeSplits[cascade] ) {
            ++cascade;
        }
        float visibility = ShadowPCF( recordIndex, record, cascade, world, normal, bias );
        float start = cascade == 0 ? record.mPositionRange.w : record.mCascadeSplits[cascade - 1];
        float blend = saturate( ( viewDepth - lerp( start, record.mCascadeSplits[cascade], .9 ) ) /
                                max( ( record.mCascadeSplits[cascade] - start ) * .1, .0001 ) );
        if ( blend > 0 ) {
            float next = cascade + 1 < record.mFaceCount
                             ? ShadowPCF( recordIndex, record, cascade + 1, world, normal, bias )
                             : 1;
            visibility = lerp( visibility, next, blend );
        }
        // Use the stronger occlusion instead of counting the same canopy twice.
        return min( visibility, contact );
    }
    if ( light.mActiveLightType == LIGHT_TYPE_POINT ) {
        float3 delta = world - record.mPositionRange.xyz;
        float distance = length( delta );
        if ( distance < 1e-5 || distance >= record.mPositionRange.w ) {
            return 1;
        }
        float3 direction = delta / distance;
        float3 up = abs( direction.y ) < .99 ? float3( 0, 1, 0 ) : float3( 0, 0, 1 );
        float3 tangent = normalize( cross( direction, up ) );
        float3 bitangent = cross( direction, tangent );
        uint width;
        uint height;
        textures2D[NonUniformResourceIndex( record.TextureID( 0 ) )].GetDimensions( width, height );
        float filterRadius = 2.0 / width;
        if ( record.mBias.w != 0 && record.mBias.z > 0 ) {
            float blockers = 0;
            float blockerSum = 0;
            float search = min( record.mBias.z / distance, 0.2 );
            for ( uint i = 0; i < kShadowDiskSampleCount; ++i ) {
                float2 offset = ShadowDisk( i ) * search;
                float3 ray = normalize( direction + tangent * offset.x + bitangent * offset.y );
                uint face = PointShadowFace( ray );
                float4x4 matrix = LoadShadowMatrix( recordIndex, face );
                float4 clip = mul( matrix, float4( record.mPositionRange.xyz + ray * distance, 1 ) );
                float2 uv = float2( clip.x / clip.w * .5 + .5, .5 - clip.y / clip.w * .5 );
                int2 pixel = clamp( int2( uv * float2( width, height ) ), 0, int2( width, height ) - 1 );
                float stored = textures2D[NonUniformResourceIndex( record.TextureID( face ) )].Load( int3( pixel, 0 ) ).r;
                float receiver = LocalShadowReceiverDepth( matrix, ( float2( pixel ) + 0.5 ) / float2( width, height ), world, normal, record.mPositionRange );
                if ( receiver > 0 && stored < receiver - bias ) {
                    blockerSum += stored * record.mPositionRange.w;
                    blockers += 1;
                }
            }
            if ( blockers == 0 ) {
                return 1;
            }
            float blocker = blockerSum / blockers;
            filterRadius = min( record.mBias.z * max( distance - blocker, 0 ) / max( blocker * distance, 0.0001 ), 0.2 );
            float visibility = 0;
            for ( uint i = 0; i < kShadowDiskSampleCount; ++i ) {
                float2 offset = ShadowDisk( i ) * filterRadius;
                float3 ray = normalize( direction + tangent * offset.x + bitangent * offset.y );
                visibility += ShadowTexel( recordIndex, record, PointShadowFace( ray ), record.mPositionRange.xyz + ray * distance, world, normal, bias );
            }
            return visibility / float( kShadowDiskSampleCount );
        }
        float sum = 0;
        // Re-select the face for every filter ray, including across cube seams.
        for ( int y = -kShadowPcfRadius; y <= kShadowPcfRadius; ++y ) {
            for ( int x = -kShadowPcfRadius; x <= kShadowPcfRadius; ++x ) {
                float3 ray = normalize( direction + ( tangent * x + bitangent * y ) * ( 2.0 / width ) );
                sum += ShadowTexel( recordIndex, record, PointShadowFace( ray ),
                    record.mPositionRange.xyz + ray * distance, world, normal, bias );
            }
        }
        return sum / float( kShadowPcfTapCount );
    }
    return ShadowPCF( recordIndex, record, 0, world, normal, bias );
}

[shader( "fragment" )]
FSOutput main( FSInput input ) {
    FSOutput output = ( FSOutput )0;

    BufferReference<MaterialParameters> materialsBuffer = ( BufferReference<MaterialParameters> )parameters.mMaterialsPtr;
    MaterialParameters material = materialsBuffer[input.mMaterialIndex];
    input.mTexCoord0 = input.mTexCoord0 * material.mUvTransform.xy + material.mUvTransform.zw;
    input.mTexCoord1 = input.mTexCoord1 * material.mUvTransform.xy + material.mUvTransform.zw;

    // ddx measures how UVs change horizontally across screen pixels; ddy measures
    // the vertical change. These gradients guide mip selection and texture filtering.
    // Sample normally computes them implicitly. Save them before discard and branches,
    // then pass them to SampleGrad later so those samples do not need to calculate
    // derivatives inside control flow where neighboring pixels may take different paths.
    // This lets metallic/roughness and AO reads happen after alpha rejection, skipping
    // those texture reads for discarded fragments.
    // Capture texture derivatives before material-dependent branches and alpha rejection.
    // Explicit gradients preserve mip selection when neighboring fragments take different
    // control-flow paths.
    float2 uv0Dx = ddx( input.mTexCoord0 );
    float2 uv0Dy = ddy( input.mTexCoord0 );
    float2 uv1Dx = ddx( input.mTexCoord1 );
    float2 uv1Dy = ddy( input.mTexCoord1 );

    // Evaluate derivatives before discard and divergent light loops. Normal
    // maps and interpolated vertex normals do not describe the rasterized plane.
    float3 receiverNormal = cross( ddx( input.mFragmentWorldPos ), ddy( input.mFragmentWorldPos ) );
    float receiverNormalLength = length( receiverNormal );
    receiverNormal = receiverNormalLength > 1e-12 ? receiverNormal / receiverNormalLength : normalize( input.mNormal );

    float3 flatNormal = SurfaceFaceNormal( ddx( input.mFragmentWorldPos ), ddy( input.mFragmentWorldPos ), input.mNormal );
    if ( MaterialBackfaceDiscard( input.mFaceFlags, input.mIsFrontFace ) ) {
        discard;
    }

    uint shadingModel = uint( material.mSubsurface.z );
    // Ray lighting currently evaluates only the base PBR BRDF. These models
    // retain raster lights/shadows and IBL instead of silently losing their lobes.
    bool useRayLighting = !SurfaceRasterModel( shadingModel ) && shadingModel != 5;
    CameraInfo camera = ( ( BufferReference<CameraInfo> )parameters.mCameraPtr )[0];

    // Fetch textures and samplers as needed, based on the material properties
    SamplerState samplerCube = samplers[parameters.mSamplerCubeID];// Linear, clamped environment/LUT sampling.
    TextureCube prefilteredMap = texturesCube[parameters.mPrefilterCubeRtID];
    TextureCube irradianceMap = texturesCube[parameters.mIrradianceCubeRtID];

    Texture2D brdfLUT = textures2D[parameters.mBrdfColorTargetID];

    SamplerState samplerBasic = samplers[parameters.mSamplerBasicID];// Normal 2D samplers

    float3 heightDu = 0;
    float3 heightDv = 0;
    float2 heightUV = material.mHeightUVSet == 0 ? input.mTexCoord0 : input.mTexCoord1;
    bool heightFrame = HeightTangents( ddx( input.mFragmentWorldPos ), ddy( input.mFragmentWorldPos ),
        material.mHeightUVSet == 0 ? uv0Dx : uv1Dx, material.mHeightUVSet == 0 ? uv0Dy : uv1Dy, heightDu, heightDv );
    if ( material.mHeightIndex >= 0 && uint( material.mHeightMapping.z ) == 2 && heightFrame ) {
        heightUV = HeightParallax( textures2D[NonUniformResourceIndex( material.mHeightIndex )], samplerBasic, heightUV,
            normalize( camera.mViewPosition.xyz - input.mFragmentWorldPos ), normalize( input.mNormal ), heightDu, heightDv,
            material.mHeightMapping.x, material.mHeightMapping.y, uint( material.mHeightMapping.w ) );
        if ( material.mHeightUVSet == 0 ) {
            input.mTexCoord0 = heightUV;
        }
        else {
            input.mTexCoord1 = heightUV;
        }
    }

    // Resolve the selected material workflow into base color, metalness and perceptual
    // roughness. Texture factors, procedural checker coverage and vertex tint are applied
    // before coverage rejection.
    float4 baseColor = float4( 0.0, 0.0, 0.0, 0.0 );

    float metallic = 0.0f;
    float perceptualRoughness = 0.0;

    if ( material.mWorkflow == PBR_WORKFLOW_METALLIC_ROUGHNESS ) {
        // Metallic and Roughness material properties are packed together
        // In glTF, these factors can be specified by fixed scalar values
        // or from a metallic-roughness map
        perceptualRoughness = material.mRoughnessFactor;
        metallic = material.mMetallicFactor;

        // The albedo may be defined from a base texture or a flat color
        if ( material.mAlbedoIndex != INVALID_TEXTURE_INDEX ) {
            Texture2D colorMap = textures2D[NonUniformResourceIndex( material.mAlbedoIndex )];
            baseColor = SRGBtoLINEAR( colorMap.Sample( samplerBasic, material.mBaseColorTextureSet == 0 ? input.mTexCoord0 : input.mTexCoord1 ) ) * material.mBaseColorFactor;
        }
        else {
            baseColor = material.mBaseColorFactor;
        }
    }

    if ( material.mWorkflow == PBR_WORKFLOW_SPECULAR_GLOSSINESS ) {
        // RGB and alpha belong to one texel; retain the raw alpha while
        // decoding only colour channels for the specular/glossiness workflow.
        float4 specGloss = float4( 1.0 );
        if ( material.mSpecularGlossinessIndex != INVALID_TEXTURE_INDEX ) {
            specGloss = textures2D[NonUniformResourceIndex( material.mSpecularGlossinessIndex )].Sample(
                samplerBasic, material.mSpecularGlossinessSet == 0 ? input.mTexCoord0 : input.mTexCoord1 );
        }
        perceptualRoughness = 1.0 - specGloss.a;
        const float epsilon = 1e-6;
        float4 diffuse = material.mDiffuseIndex != INVALID_TEXTURE_INDEX
                             ? SRGBtoLINEAR( textures2D[NonUniformResourceIndex( material.mDiffuseIndex )].Sample(
                                   samplerBasic, ( material.mBaseColorTextureSet == 0 ? input.mTexCoord0 : input.mTexCoord1 ) ) )
                             : float4( 1.0 );
        float3 specular = SRGBtoLINEAR( specGloss ).rgb;

        float maxSpecular = max( max( specular.r, specular.g ), specular.b );

        // Convert metallic value from specular glossiness inputs
        metallic = ConvertMetallic( diffuse.rgb, specular, maxSpecular );

        float3 baseColorDiffusePart = diffuse.rgb *
                                      ( ( 1.0 - maxSpecular ) / ( 1 - MIN_ROUGHNESS ) / max( 1 - metallic, epsilon ) ) *
                                      material.mDiffuseFactor.rgb;
        float3 baseColorSpecularPart =
            specular - ( float3( MIN_ROUGHNESS ) * ( 1 - metallic ) * ( 1 / max( metallic, epsilon ) ) ) *
                           material.mSpecularFactor.rgb;
        baseColor = float4( lerp( baseColorDiffusePart, baseColorSpecularPart, metallic * metallic ),
            diffuse.a * material.mDiffuseFactor.a );
    }

    if ( material.mCheckerboardColorA.w != 0.0 ) {
        baseColor.rgb *= CheckerboardColor( input.mFragmentWorldPos, fwidth( input.mFragmentWorldPos ), flatNormal,
            material.mCheckerboardColorA.rgb, material.mCheckerboardColorB, material.mCheckerboardOffset.xyz );
    }

    baseColor *= float4( input.mVertexColor );
    baseColor.a = saturate( baseColor.a );
    // Opaque BLEND pixels were depth-tested with the opaque scene. Shade them
    // there once; only fractional coverage belongs in the sorted blend pass.
    bool transparentPass = ( parameters.mPrefilterMipLevelsAndSkybox & PBR_TRANSPARENT_SURFACE_PASS ) != 0;
    if ( MaterialSurfaceDiscard( material, baseColor.a, transparentPass ) ) {
        discard;
    }
    // Rejected cutout/blend fragments never need roughness or metallic data.
    // Keep the full texel so a matching packed AO channel can reuse this read.
    float4 mrSample = float4( 1.0 );
    if ( material.mWorkflow == PBR_WORKFLOW_METALLIC_ROUGHNESS ) {
        if ( material.mMetallicRoughnessIndex != INVALID_TEXTURE_INDEX ) {
            // Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
            // This layout intentionally reserves the 'r' channel for (optional) occlusion map data
            Texture2D metallicRoughnessMap = textures2D[NonUniformResourceIndex( material.mMetallicRoughnessIndex )];
            mrSample = metallicRoughnessMap.SampleGrad(
                samplerBasic, material.mMetallicRoughnessTextureSet == 0 ? input.mTexCoord0 : input.mTexCoord1,
                material.mMetallicRoughnessTextureSet == 0 ? uv0Dx : uv1Dx,
                material.mMetallicRoughnessTextureSet == 0 ? uv0Dy : uv1Dy );

            perceptualRoughness = mrSample.g * perceptualRoughness;
            metallic = mrSample.b * metallic;
        }
        else {
            perceptualRoughness = clamp( perceptualRoughness, MIN_ROUGHNESS, 1.0 );
            metallic = clamp( metallic, 0.0, 1.0 );
        }
    }
    // Opaque and surviving cutout fragments fully cover their pixel. Only BLEND
    // materials pass fractional opacity to the transparent pipeline.
    if ( material.mAlphaMask != PBR_ALPHA_BLEND ) {
        baseColor.a = 1.0f;
    }

    uint cullingClass = ( input.mFaceFlags >> 2 ) & 3u;
    if ( cullingClass != 0 ) {
        float3 tint = cullingClass == 1   ? float3( 0.3, 0.9, 0.06 )
                      : cullingClass == 2 ? float3( 0.9, 0.04, 0.025 )
                                          : float3( 1.0, 0.55, 0.04 );
        // Preserve material cutouts, sidedness and transparent opacity in debug mode.
        output.mColor = float4( tint, baseColor.a );
        return output;
    }

    if ( shadingModel == SURFACE_UNLIT ) {
        float3 emission = material.mEmissiveFactor.rgb * material.mEmissiveStrength;
        if ( material.mEmissiveIndex != INVALID_TEXTURE_INDEX ) {
            emission *= SRGBtoLINEAR( textures2D[NonUniformResourceIndex( material.mEmissiveIndex )].Sample(
                                          samplerBasic, material.mEmissiveTextureSet == 0 ? input.mTexCoord0 : input.mTexCoord1 ) )
                            .rgb;
        }
        output.mColor = float4( baseColor.rgb + emission, baseColor.a );
        return output;
    }

    // dielectrics default to 4%
    // Build the material BRDF inputs. Metallic color and dielectric interface reflectance
    // determine how much energy enters diffuse, specular and transmission lobes.
    float3 f0 = float3( 0.04 );
    float transmission = 0;
    if ( shadingModel == SURFACE_DIELECTRIC ) {
        float ior = max( material.mSurfaceShading.x, 1.0 );
        float ratio = ( ior - 1 ) / ( ior + 1 );
        f0 = float3( ratio * ratio );
        metallic = 0;
        transmission = saturate( material.mSurfaceShading.y );
    }

    // diffuse should disappear as the material becomes metallic.
    float diffuseFactor = ( 1.0f - metallic ) * ( 1 - transmission );

    // The reference convolves colored diffuse radiance, preserving specular and
    // ambient in separate buffers. No division by albedo or HDR subtraction.
    float3 diffuseAlbedo = baseColor.rgb;
    float3 diffuseColor = diffuseAlbedo * ( float3( 1.0 ) - f0 ) * diffuseFactor;
    float3 specularColor = lerp( f0, baseColor.rgb, metallic );

    float alphaRoughness = perceptualRoughness * perceptualRoughness;
    float reflectance = max( max( specularColor.r, specularColor.g ), specularColor.b );

    // For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
    // For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
    float reflectance90 = clamp( reflectance * 25.0, 0.0, 1.0 );
    if ( shadingModel == SURFACE_DIELECTRIC ) {
        reflectance90 = 1.0;
    }
    float3 specularEnvironmentR0 = specularColor.rgb;
    float3 specularEnvironmentR90 = float3( 1.0, 1.0, 1.0 ) * reflectance90;

    float2 normalUvSet = material.mNormalTextureSet == 0 ? input.mTexCoord0 : input.mTexCoord1;
    float3 n = ( shadingModel != SURFACE_FLAT && material.mNormalIndex != INVALID_UV_SET )
                   ? GetNormalFromMap( textures2D[NonUniformResourceIndex( material.mNormalIndex )], samplerBasic,
                         normalUvSet, input.mNormal, input.mFragmentWorldPos, material.mNormalScale )
                   : normalize( input.mNormal );

    float3 coatNormal = normalize( input.mNormal );
    if ( shadingModel == SURFACE_CLEAR_COAT && material.mClearCoatNormalIndex >= 0 ) {
        float2 coatUv = material.mClearCoatNormalTextureSet == 0 ? input.mTexCoord0 : input.mTexCoord1;
        coatNormal = GetClearCoatNormalFromMap( textures2D[NonUniformResourceIndex( material.mClearCoatNormalIndex )],
            samplerBasic, coatUv, input.mNormal, input.mFragmentWorldPos, material.mClearCoatNormalScale );
    }
    if ( shadingModel == SURFACE_FLAT ) {
        n = flatNormal;
    }
    if ( !MaterialFrontFacing( input.mFaceFlags, input.mIsFrontFace ) ) {
        n = -n;
        coatNormal = -coatNormal;
    }

    if ( material.mHeightIndex >= 0 && material.mHeightMapping.z != 0 && heightFrame ) {
        n = HeightBumpNormal( textures2D[NonUniformResourceIndex( material.mHeightIndex )], samplerBasic, heightUV,
            n, heightDu, heightDv, material.mHeightMapping.x );
    }
    // Finish the world-space shading frame and derive the view/reflection directions. The
    // direct-light and environment terms consume this same normal/roughness convention.
    float3 v = normalize( camera.mViewPosition.xyz - input.mFragmentWorldPos );// Vector from surface point to camera
    float3 reflection = normalize( reflect( -v, n ) );

    BaseInputs inputs;
    inputs.mCoatNormal = coatNormal;
    inputs.mModel = shadingModel;
    inputs.mSettings = material.mSurfaceShading;
    inputs.mBands = material.mSubsurface.w;
    inputs.mPerceptualRoughness = perceptualRoughness;
    inputs.mMetallic = metallic;
    inputs.mSpecularEnvironmentR0 = specularEnvironmentR0;
    inputs.mSpecularEnvironmentR90 = specularEnvironmentR90;
    inputs.mAlphaRoughness = alphaRoughness;
    inputs.mDiffuseColor = diffuseColor;
    inputs.mSpecularColor = specularColor;

    // Accumulate direct illumination only from the pixel cluster light list. Each light
    // contribution is weighted by its geometry and participating-medium visibility.
    float3 lightContribution = float3( 0.0 );

    // Cluster resolve
    // TODO: Check first if there are active lights
    BufferReference<Cluster> clusterBuffer = ( BufferReference<Cluster> )parameters.mClusterPtr;
    BufferReference<LightInfo> lightCullingBuffer = ( BufferReference<LightInfo> )parameters.mLightsPtr;

    float3 gridSize = float3( parameters.mGridSize1.xy, parameters.mGridSizeZ );

    uint clusterIndex = ComputeTileIndex( input.mFragmentViewPos, gridSize, camera.mPlanes.xy, input.mScreenPos,
        camera.mScreenDimensions.xy );

    if ( clusterIndex < parameters.mClusterBufferSize ) {
        // Overflow falls back to the complete list rather than dropping lights.
        ClusterHeader cluster = LoadClusterHeader( clusterBuffer.address, clusterIndex );
        bool overflow = cluster.mPadding[0] != 0;
        uint count = overflow ? parameters.mActiveLightCount : min( cluster.mCount, MAX_LIGHT_CLUSTERS );
        for ( uint i = 0; i < count; ++i ) {
            uint lightIndex = overflow ? i : LoadClusterLight( clusterBuffer.address, clusterIndex, i );
            if ( lightIndex >= parameters.mActiveLightCount ) {
                continue;
            }

            LightInfo light = lightCullingBuffer[lightIndex];
            if ( light.mActiveLightType == LIGHT_TYPE_DIRECTIONAL && light.mDirection.w > 0.5 && camera.mAtmosphere != 0 ) {
                AtmosphereData atmosphere = ( ( BufferReference<AtmosphereData> )camera.mAtmosphere )[0];
                light.mDiffuse.xyz = AtmosSurfaceSun( atmosphere, input.mFragmentWorldPos,
                    textures2D[camera.mAtmosphereTransmittance], samplers[camera.mAtmosphereSampler] );
            }

            if ( light.mActiveLightType == LIGHT_TYPE_DIRECTIONAL ) {
                light.mDiffuse.rgb *= CloudSurfaceShadow( camera.mClouds, input.mFragmentWorldPos, normalize( -light.mDirection.xyz ) );
            }

            // Hybrid direct lighting replaces punctual lights only; analytic raster area lights remain additive.
            if ( useRayLighting && parameters.mRayDirect != 0xffffffffu && light.mActiveLightType != LIGHT_TYPE_AREA ) {
                continue;
            }

            float3 contribution = float3( 0 );

            bool stylized = SurfaceStylized( shadingModel );

            float visibility = stylized ? LightShadow( light, input.mFragmentWorldPos, receiverNormal, -input.mFragmentViewPos.z, input.mScreenPos.xyz ) : 1.0;
            if ( light.mActiveLightType == LIGHT_TYPE_DIRECTIONAL ) {
                contribution = ComputeDirectionalLightContribution( n, v, light, inputs, visibility );
            }
            else if ( light.mActiveLightType == LIGHT_TYPE_POINT ) {
                contribution = ComputePointlLightContribution( n, v, light, inputs, input.mFragmentWorldPos, visibility );
            }
            else if ( light.mActiveLightType == LIGHT_TYPE_SPOT ) {
                contribution = ComputeSpotLightContribution( n, v, light, inputs, input.mFragmentWorldPos, visibility );
            }

            if ( light.mActiveLightType == LIGHT_TYPE_AREA ) {
                AreaSurfaceLighting area = EvaluateAreaLight( light, input.mFragmentWorldPos, n, v,
                    inputs.mDiffuseColor, inputs.mSpecularEnvironmentR0, inputs.mSpecularEnvironmentR90, inputs.mPerceptualRoughness );
                if ( light.mAreaEmission.x != 0xffffffffu ) {
                    Texture2D source = textures2D[NonUniformResourceIndex( light.mAreaEmission.x )];
                    SamplerState sourceSampler = samplers[NonUniformResourceIndex( light.mAreaEmission.y )];
                    area.mDiffuse *= AreaDiffuseTexture( light, input.mFragmentWorldPos, n, source, sourceSampler );
                    area.mSpecular *= AreaSpecularTexture( light, area, inputs.mPerceptualRoughness, source, sourceSampler );
                }
                float3 radiance = light.mDiffuse.xyz * light.mIntensity;
                contribution = radiance * SurfaceDirect(
                                              area.mDiffuse + area.mSpecular, inputs.mDiffuseColor, inputs.mSpecularColor, inputs.mPerceptualRoughness,
                                              n, v, area.mDirection, inputs.mModel, inputs.mSettings, inputs.mBands, visibility, inputs.mCoatNormal, area.mWeight );
            }

            // Range, cone and surface-facing tests can eliminate the light.
            // Do not fetch its PCF taps when visibility cannot affect the result.
            if ( any( contribution != float3( 0.0 ) ) ) {
                float shadow = stylized ? 1.0 : LightShadow( light, input.mFragmentWorldPos, receiverNormal, -input.mFragmentViewPos.z, input.mScreenPos.xyz );
                float3 mediumTransmittance = 1.0;
                if ( VolumeCount( camera.mVolumes ) != 0 ) {
                    bool directional = light.mActiveLightType == LIGHT_TYPE_DIRECTIONAL;
                    float3 delta = directional ? -light.mDirection.xyz : light.mPosition.xyz - input.mFragmentWorldPos;
                    mediumTransmittance = VolumeTransmittance( camera.mVolumes, input.mFragmentWorldPos,
                        normalize( delta ), directional ? 1e30 : length( delta ) );
                }
                lightContribution += contribution * shadow * mediumTransmittance;
            }
        }
    }

    if ( useRayLighting && parameters.mRayDirect != 0xffffffffu ) {
        lightContribution += textures2D[parameters.mRayDirect].Load( int3( int2( input.mScreenPos.xy ), 0 ) ).rgb;
    }

    // IBL Contribution
    float coatNdotV = clamp( dot( coatNormal, v ), 0.001, 1.0 );
    float NdotV = clamp( dot( n, v ), 0.001, 1.0 );
    // Add the environment contribution after direct lights. SSAO and material occlusion
    // modulate the indirect term rather than darkening direct illumination indiscriminately.
    float3 iblLighting = float3( 0.0 );
    float3 environmentSpecular = float3( 0.0 );

    if ( ( parameters.mPrefilterMipLevelsAndSkybox & 0x80000000u ) != 0u ) {
        iblLighting = GetIBLContribution( NdotV, diffuseColor, specularColor, perceptualRoughness, n, reflection,
            brdfLUT, irradianceMap, prefilteredMap, samplerCube, useRayLighting, environmentSpecular );
        if ( SurfaceStylized( shadingModel ) ) {
            iblLighting = irradianceMap.SampleLevel( samplerCube, n, 0 ).rgb * diffuseColor * parameters.mScaleIblAmbient;
        }
        else if ( shadingModel == SURFACE_CLEAR_COAT && material.mSurfaceShading.x > 0 ) {
            float weight = material.mSurfaceShading.x;
            float f = SurfaceCoatFresnel( coatNdotV );
            float roughness = material.mSurfaceShading.y;
            float lod = roughness * max( float( parameters.mPrefilterMipLevelsAndSkybox & 0x07ffffffu ) - 1, 0 );
            float2 brdf = brdfLUT.SampleLevel( samplerCube, float2( coatNdotV, 1 - roughness ), 0 ).rg;
            float3 coat = prefilteredMap.SampleLevel( samplerCube, reflect( -v, coatNormal ), lod ).rgb * ( 0.04 * brdf.x + brdf.y );
            iblLighting = iblLighting * ( 1 - weight * f ) + weight * coat * parameters.mScaleIblAmbient;
        }
    }

    float ambientOcclusion = 1.0;

    // AO only modulates IBL here. Direct light and ray-traced indirect light
    // do not consume it, so disabled IBL needs neither AO texture read.
    if ( ( parameters.mPrefilterMipLevelsAndSkybox & 0x80000000u ) != 0u ) {
        // Screen-space and material occlusion affect ambient illumination only.
        // Multiplying by color a second time used to square and darken the result.
        if ( parameters.mSsaoTextureID != 0xFFFFFFFFu ) {
            uint width;
            uint height;
            textures2D[parameters.mSsaoTextureID].GetDimensions( width, height );
            int2 pixel = clamp( int2( input.mScreenPos.xy ), int2( 0 ), int2( width, height ) - 1 );
            ambientOcclusion = pow( saturate( textures2D[parameters.mSsaoTextureID].Load( int3( pixel, 0 ) ).r ),
                max( parameters.mSsaoIntensity, 0.0 ) );
        }

        if ( material.mAoIndex != INVALID_TEXTURE_INDEX ) {
            float2 aoUV = material.mOcclusionTextureSet == 0 ? input.mTexCoord0 : input.mTexCoord1;
            bool sharedOrm = material.mWorkflow == PBR_WORKFLOW_METALLIC_ROUGHNESS &&
                             material.mAoIndex == material.mMetallicRoughnessIndex &&
                             material.mOcclusionTextureSet == material.mMetallicRoughnessTextureSet;
            if ( sharedOrm ) {
                ambientOcclusion *= mrSample.r;
            }
            else {
                ambientOcclusion *= textures2D[NonUniformResourceIndex( material.mAoIndex )].SampleGrad(
                                                                                               samplerBasic, aoUV, material.mOcclusionTextureSet == 0 ? uv0Dx : uv1Dx,
                                                                                               material.mOcclusionTextureSet == 0 ? uv0Dy : uv1Dy )
                                        .r;
            }
        }
    }

    float3 color = lightContribution + iblLighting * ambientOcclusion;
    if ( shadingModel == SURFACE_DIELECTRIC && transmission > 0 ) {
        // Environment transmission is a thin-volume approximation for ordinary
        // glass meshes. Water's separate shader additionally samples scene color.
        float ior = max( material.mSurfaceShading.x, 1.0 );
        bool entering = MaterialFrontFacing( input.mFaceFlags, input.mIsFrontFace );
        float3 ray = refract( -v, n, entering ? 1 / ior : ior );
        if ( dot( ray, ray ) > 1e-8 ) {
            float distance = material.mSurfaceShading.z / max( abs( dot( ray, n ) ), 0.1 );
            float3 absorption = pow( max( material.mSubsurfaceColorOpacity.rgb, 0.0001 ),
                distance / max( material.mSurfaceShading.w, 0.001 ) );
            float fresnel = f0.x + ( 1 - f0.x ) * pow( 1 - saturate( dot( n, v ) ), 5.0 );
            if ( ( parameters.mPrefilterMipLevelsAndSkybox & 0x80000000u ) != 0 ) {
                float maxLod = max( float( parameters.mPrefilterMipLevelsAndSkybox & 0x07ffffffu ) - 1, 0 );
                color += prefilteredMap.SampleLevel( samplerCube, ray, perceptualRoughness * maxLod ).rgb *
                         absorption * baseColor.rgb * transmission * ( 1 - fresnel ) * parameters.mScaleIblAmbient;
            }
        }
    }
    if ( useRayLighting && parameters.mRayIndirect != 0xffffffffu ) {
        color += textures2D[parameters.mRayIndirect].Load( int3( int2( input.mScreenPos.xy ), 0 ) ).rgb;
    }

    // Emission
    // Emission adds radiance independently of incoming lights. Material emission maps and
    // strength remain in scene-linear HDR until later tone mapping.
    float3 emissiveContrib = material.mEmissiveFactor.xyz * material.mEmissiveStrength;
    if ( material.mEmissiveIndex != INVALID_TEXTURE_INDEX ) {
        Texture2D emissiveMap = textures2D[NonUniformResourceIndex( material.mEmissiveIndex )];
        float2 emissiveUvSet = material.mEmissiveTextureSet == 0 ? input.mTexCoord0 : input.mTexCoord1;

        // Emissive textures are sRGB, just as in the bloom/G-buffer pass.
        float3 emissive = SRGBtoLINEAR( emissiveMap.Sample( samplerBasic, emissiveUvSet ) ).rgb;

        emissiveContrib *= emissive;
    }

    if ( shadingModel == SURFACE_CLEAR_COAT ) {
        emissiveContrib *= 1 - material.mSurfaceShading.x * SurfaceCoatFresnel( coatNdotV );
    }

    color += emissiveContrib;
    if ( camera.mVisualizeCascades != 0 ) {
        // Visualize the first shadowed directional light, using precisely the
        // same split selection and blend interval as LightShadow.
        const float3 palette[4] = { float3( 0.9, 0.15, 0.12 ), float3( 0.15, 0.8, 0.25 ), float3( 0.15, 0.35, 1.0 ),
            float3( 0.9, 0.7, 0.15 ) };
        float viewDepth = -input.mFragmentViewPos.z;
        float3 tint = float3( 0.15 );
        for ( uint index = 0; index < parameters.mShadowRecordCount; ++index ) {
            ShadowMetadata record = LoadShadowMetadata( index );
            if ( record.mLightType != LIGHT_TYPE_DIRECTIONAL || record.mFaceCount == 0 ) {
                continue;
            }
            if ( viewDepth > 0.0 && viewDepth <= record.mCascadeSplits.w ) {
                uint cascade = 0;
                while ( cascade + 1 < record.mFaceCount && viewDepth > record.mCascadeSplits[cascade] ) {
                    ++cascade;
                }
                float start = cascade == 0 ? record.mPositionRange.w : record.mCascadeSplits[cascade - 1];
                float blend = saturate( ( viewDepth - lerp( start, record.mCascadeSplits[cascade], 0.9 ) ) /
                                        max( ( record.mCascadeSplits[cascade] - start ) * 0.1, 0.0001 ) );
                tint = lerp( palette[min( cascade, 3u )],
                    cascade + 1 < record.mFaceCount ? palette[min( cascade + 1, 3u )] : float3( 0.15 ), blend );
            }
            break;
        }

        // Tint the fully shaded scene so textures and shadows remain visible.
        // Multiplication preserves dark shadows without adding debug emission.
        color *= lerp( float3( 1.0 ), tint, 0.75 );
    }

    // Alpha-one pixels of BLEND materials use the opaque pass and FogComposite.
    // Only translucent fragments apply fog here, so each surface receives it once.
    // Transparent surfaces need fog evaluated at their own distance because their depth may
    // differ from the opaque prepass receiver used by the fullscreen fog pass.
    if ( transparentPass && camera.mFogFrame != 0 ) {
        FogFrameData fog = LoadBufferValue<FogFrameData>( camera.mFogFrame, 0 );
        if ( fog.mTextures.w != 0 ) {
            float4 clip = CameraClipPosition( float4( input.mFragmentWorldPos, 1.0 ), camera );
            float2 uv = float2( clip.x / clip.w * 0.5 + 0.5, 0.5 - clip.y / clip.w * 0.5 );
            float3 origin;
            float3 direction;
            FogCameraRay( camera.mInverseViewProjection, camera.mViewPosition.xyz, camera.mProjection[3][3] != 0.0,
                uv, origin, direction );
            float3 radiance;
            float3 transmittance;
            ReadIntegratedFog( textures2D[fog.mTextures.x], textures2D[fog.mTextures.y], samplers[fog.mTextures.z],
                uv, length( input.mFragmentWorldPos - origin ), fog, radiance, transmittance );
            color = color * transmittance + radiance;
        }
    }
#if defined( MKT_SSR_CAPTURE )
    // Reuse the material's actual split-sum response. Keep multi-lobe clear coat
    // and stylized shading on their existing environment path until traced per lobe.
    if ( !SurfaceStylized( shadingModel ) && shadingModel != SURFACE_CLEAR_COAT ) {
        float2 integratedBrdf = brdfLUT.SampleLevel( samplerCube, float2( NdotV, 1 - perceptualRoughness ), 0 ).rg;
        output.mSurfaceResponse = float4( specularColor * integratedBrdf.x + integratedBrdf.y, perceptualRoughness );
        output.mEnvironmentSpecular = float4( environmentSpecular * ambientOcclusion, 1 );
    }
#endif
    output.mColor = float4( color, baseColor.a );

    return output;
}
