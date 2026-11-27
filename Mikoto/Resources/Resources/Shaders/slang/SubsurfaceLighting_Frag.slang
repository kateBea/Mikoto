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

// MRT capture for the diffuse -> horizontal -> vertical -> composite pipeline.
// https://bentobaux.github.io/posts/separable-subsurface-scattering-in-unity-6/
// Further ref: https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/glTF-WebGL-PBR/shaders/pbr-frag.glsl
// https://github.com/KhronosGroup/glTF-Sample-Viewer.git (branch glTF-WebGL-PBR)

// Evaluate surface illumination while separating the diffuse contribution that will be
// spatially scattered. Shadow visibility and material evaluation agree with the regular forward
// surface path.

#pragma once

#include "base/AreaLighting_Helpers.slang"
#include "base/Atmosphere_Helpers.slang"
#include "base/Base.slang"
#include "base/Camera_Helpers.slang"
#include "base/Clouds_Helpers.slang"
#include "base/ClusteringLights_Helpers.slang"
#include "base/Lighting_Helpers.slang"
#include "base/Material_Helpers.slang"
#include "base/Math_Helpers.slang"
#include "base/Memory_Helpers.slang"
#include "base/ShadowMap_Helpers.slang"
#include "base/SurfaceShading_Helpers.slang"
#include "base/Tonemap_Helpers.slang"

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

// Encapsulate the various inputs used by the various functions in the shading equation
// We store values in this struct to simplify the integration of alternative implementations
// of the shading terms, outlined in the Readme.MD Appendix.
struct PBRInfo {
    float mNdotL;// cos angle between normal and light direction
    float mNdotV;// cos angle between normal and view direction
    float mNdotH;// cos angle between normal and half vector
    float mLdotH;// cos angle between light direction and half vector
    float mVdotH;// cos angle between view direction and half vector

    float mPerceptualRoughness;// roughness value, as authored by the model creator (input to shader)
    float mMetalness;          // metallic value at the surface
    float3 mReflectance0;      // full reflectance color (normal incidence angle)
    float3 mReflectance90;     // reflectance color at grazing angle
    float mAlphaRoughness;     // roughness mapped to a more linear change in the roughness (proposed by [2])
    float3 mDiffuseColor;      // color contribution from diffuse lighting
    float3 mSpecularColor;     // color contribution from specular lighting
};

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
    SamplerState sampler, bool useRayLighting ) {
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
    return ( rayDiffuse ? float3( 0.0 ) : diffuse ) + ( raySpecular ? float3( 0.0 ) : specular );
}

// Basic Lambertian diffuse
// Implementation from Lambert's Photometria https://archive.org/details/lambertsphotome00lambgoog
float3 Diffuse( PBRInfo pbrInputs ) {
    return pbrInputs.mDiffuseColor / PI_Value;
}

// Disney Implementation of diffuse from Physically-Based Shading at Disney by Brent Burley. See Section 5.3.
// http://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
float3 DiffuseBurley( PBRInfo pbrInputs ) {
    float f90 = 2.0 * pbrInputs.mLdotH * pbrInputs.mLdotH * pbrInputs.mAlphaRoughness - 0.5;

    return ( pbrInputs.mDiffuseColor / PI_Value ) * ( 1.0 + f90 * pow( ( 1.0 - pbrInputs.mNdotL ), 5.0 ) ) *
           ( 1.0 + f90 * pow( ( 1.0 - pbrInputs.mNdotV ), 5.0 ) );
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
float3 SpecularReflection( PBRInfo pbrInputs ) {
    return pbrInputs.mReflectance0 +
           ( pbrInputs.mReflectance90 - pbrInputs.mReflectance0 ) * pow( clamp( 1.0 - pbrInputs.mVdotH, 0.0, 1.0 ), 5.0 );
}

// This calculates the specular geometric attenuation (aka G()),
// where rougher material will reflect less light back to the viewer.
float GeometricOcclusion( PBRInfo pbrInputs ) {
    float NdotL = pbrInputs.mNdotL;
    float NdotV = pbrInputs.mNdotV;
    float r = pbrInputs.mAlphaRoughness;

    float attenuationL = 2.0 * NdotL / ( NdotL + sqrt( r * r + ( 1.0 - r * r ) * ( NdotL * NdotL ) ) );
    float attenuationV = 2.0 * NdotV / ( NdotV + sqrt( r * r + ( 1.0 - r * r ) * ( NdotV * NdotV ) ) );
    return attenuationL * attenuationV;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games
float MicrofacetDistribution( PBRInfo pbrInputs ) {
    float roughnessSq = pbrInputs.mAlphaRoughness * pbrInputs.mAlphaRoughness;
    float f = ( pbrInputs.mNdotH * roughnessSq - pbrInputs.mNdotH ) * pbrInputs.mNdotH + 1.0;
    return roughnessSq / ( PI_Value * f * f );
}

float3 ComputeDirectionalLightContribution( float3 n, float3 v, LightInfo lightInfo, BaseInputs inputs, out float3 sharpSpecular, float visibility = 1.0 ) {
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
    sharpSpecular = lightInfo.mDiffuse.xyz * NdotL * specContrib;
    float3 color = lightInfo.mDiffuse.xyz * SurfaceDirect(
                                               NdotL * diffuseContrib,
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

float3 ComputePointlLightContribution( float3 n, float3 v, LightInfo lightInfo, BaseInputs inputs, float3 worldPos, out float3 sharpSpecular, float visibility = 1.0 ) {
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
    sharpSpecular = lightIntensity * NdotL * specContrib;
    const float3 color = lightIntensity * SurfaceDirect(
                                              NdotL * diffuseContrib,
                                              inputs.mDiffuseColor, inputs.mSpecularColor, inputs.mPerceptualRoughness,
                                              n, v, l, inputs.mModel, inputs.mSettings, inputs.mBands, visibility, inputs.mCoatNormal );

    return color;
}

float3 ComputeSpotLightContribution( float3 n, float3 v, LightInfo lightInfo, BaseInputs inputs, float3 worldPos, out float3 sharpSpecular, float visibility = 1.0 ) {
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
    sharpSpecular = lightIntensity * NdotL * specContrib;
    float3 color = lightIntensity * SurfaceDirect(
                                        NdotL * diffuseContrib,
                                        inputs.mDiffuseColor, inputs.mSpecularColor, inputs.mPerceptualRoughness,
                                        n, v, l, inputs.mModel, inputs.mSettings, inputs.mBands, visibility, inputs.mCoatNormal );

    return color;
}

struct FSOutput {
    float4 mColor : SV_Target0;
    float4 mSpecular : SV_Target1;
    float4 mAmbient : SV_Target2;
    float4 mIdentityDepth : SV_Target3;
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

// Average nearby depth comparisons instead of filtering raw depth. The filter stays tied to
// receiver geometry so map bias and sloping surfaces use compatible depths.
// The fixed PCF neighborhood contains nine depth comparisons (3x3).
static const uint kSubsurfacePcfTapCount = 9;

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
    float sum = 0;
    for ( int y = -kShadowPcfRadius; y <= kShadowPcfRadius; ++y ) {
        for ( int x = -kShadowPcfRadius; x <= kShadowPcfRadius; ++x ) {
            sum += CompareShadowTexel( record, viewProjection, texture, int2( width, height ), pixel + int2( x, y ),
                ndc, world, normal, bias );
        }
    }
    return sum / float( kSubsurfacePcfTapCount );
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
        float sum = 0;
        // Re-select the face for every filter ray, including across cube seams.
        for ( int y = -kShadowPcfRadius; y <= kShadowPcfRadius; ++y ) {
            for ( int x = -kShadowPcfRadius; x <= kShadowPcfRadius; ++x ) {
                float3 ray = normalize( direction + ( tangent * x + bitangent * y ) * ( 2.0 / width ) );
                sum += ShadowTexel( recordIndex, record, PointShadowFace( ray ),
                    record.mPositionRange.xyz + ray * distance, world, normal, bias );
            }
        }
        return sum / float( kSubsurfacePcfTapCount );
    }
    return ShadowPCF( recordIndex, record, 0, world, normal, bias );
}

// Artist-authored thickness transmission, independently implemented from:
// https://bentobaux.github.io/posts/separable-subsurface-scattering-in-unity-6/#bonus-adding-transmission
// https://github.com/bentoBAUX/Separable-Subsurface-Scattering-in-Unity-6
// Attenuation is precomputed from near/far profiles on material edits. This
// optional backlight approximation deliberately does not estimate shadow thickness.
float3 SubsurfaceTransmission( LightInfo light, MaterialParameters material, float3 albedo,
    float3 world, float3 normal ) {
    if ( !any( material.mSubsurfaceTransmission.rgb > 0 ) ) {
        return float3( 0 );
    }
    float3 vector = light.mPosition.xyz - world;
    float distance = length( vector );
    float3 l = light.mActiveLightType == LIGHT_TYPE_DIRECTIONAL ? normalize( -light.mDirection.xyz ) : vector / max( distance, 1e-6 );
    float back = saturate( ( 0.3 - dot( l, normal ) ) / 1.3 );
    float3 radiance = light.mDiffuse.rgb;
    if ( light.mActiveLightType == LIGHT_TYPE_POINT ) {
        radiance *= light.mIntensity * GetDistanceAtt( vector, 1 / max( light.mRadius * light.mRadius, 1e-6 ) );
    }
    else if ( light.mActiveLightType == LIGHT_TYPE_SPOT ) {
        float theta = dot( normalize( -light.mDirection.xyz ), l );
        float cone = saturate( ( theta - light.mOuterCutOff ) / max( light.mCutOff - light.mOuterCutOff, 1e-6 ) );
        float fade = saturate( 1 - distance / max( light.mRadius, 1e-6 ) );
        radiance *= light.mIntensity * 1000 * fade * fade * cone;
    }
    else if ( light.mActiveLightType == LIGHT_TYPE_AREA ) {
        // Reuse the back-facing shape integral for the existing artist-thickness approximation.
        radiance *= light.mIntensity * SampleAreaLight( light, world, -normal, l, 1.0 ).mIrradiance;
        if ( light.mAreaEmission.x != 0xffffffffu ) {
            radiance *= AreaDiffuseTexture( light, world, -normal,
                textures2D[NonUniformResourceIndex( light.mAreaEmission.x )], samplers[NonUniformResourceIndex( light.mAreaEmission.y )] );
        }
        back = 1.0;
    }
    return albedo * radiance * material.mSubsurfaceTransmission.rgb * back *
           saturate( material.mSubsurfaceColorOpacity.rgb ) * saturate( material.mSubsurface.y * ( 1 - material.mSubsurfaceColorOpacity.a ) );
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
    if ( material.mSubsurface.z != 5.0 || material.mSubsurface.x <= 0.0 || material.mSubsurface.y <= 0.0 ||
         material.mAlphaMask == PBR_ALPHA_BLEND || material.mWorkflow == PBR_WORKFLOW_UNLIT || camera.mVisualizeCascades != 0 ||
         ( ( input.mFaceFlags >> 2 ) & 3u ) != 0 ) {
        discard;
    }

    // Fetch textures and samplers as needed, based on the material properties
    SamplerState samplerCube = samplers[parameters.mSamplerCubeID];// Linear, clamped environment/LUT sampling.
    TextureCube prefilteredMap = texturesCube[parameters.mPrefilterCubeRtID];
    TextureCube irradianceMap = texturesCube[parameters.mIrradianceCubeRtID];

    Texture2D brdfLUT = textures2D[parameters.mBrdfColorTargetID];

    SamplerState samplerBasic = samplers[parameters.mSamplerBasicID];// Normal 2D samplers

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
    float3 sharpSpecular = float3( 0.0 );

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
            float3 lightSpecular = float3( 0 );

            bool stylized = SurfaceStylized( shadingModel );

            float visibility = stylized ? LightShadow( light, input.mFragmentWorldPos, receiverNormal, -input.mFragmentViewPos.z, input.mScreenPos.xyz ) : 1.0;
            if ( light.mActiveLightType == LIGHT_TYPE_DIRECTIONAL ) {
                contribution = ComputeDirectionalLightContribution( n, v, light, inputs, lightSpecular, visibility );
            }
            else if ( light.mActiveLightType == LIGHT_TYPE_POINT ) {
                contribution = ComputePointlLightContribution( n, v, light, inputs, input.mFragmentWorldPos, lightSpecular, visibility );
            }
            else if ( light.mActiveLightType == LIGHT_TYPE_SPOT ) {
                contribution = ComputeSpotLightContribution( n, v, light, inputs, input.mFragmentWorldPos, lightSpecular, visibility );
            }

            lightContribution += SubsurfaceTransmission( light, material, diffuseAlbedo, input.mFragmentWorldPos, n );

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
                                              area.mDiffuse, inputs.mDiffuseColor, inputs.mSpecularColor, inputs.mPerceptualRoughness,
                                              n, v, area.mDirection, inputs.mModel, inputs.mSettings, inputs.mBands, visibility, inputs.mCoatNormal, area.mWeight );
                lightSpecular = radiance * area.mSpecular;
            }

            // Range, cone and surface-facing tests can eliminate the light.
            // Do not fetch its PCF taps when visibility cannot affect the result.
            if ( any( contribution != float3( 0.0 ) ) || any( lightSpecular != float3( 0.0 ) ) ) {
                float shadow = stylized ? 1.0 : LightShadow( light, input.mFragmentWorldPos, receiverNormal, -input.mFragmentViewPos.z, input.mScreenPos.xyz );
                lightContribution += contribution * shadow;
                sharpSpecular += lightSpecular * shadow;
            }
        }
    }

    if ( useRayLighting && parameters.mRayDirect != 0xffffffffu ) {
        lightContribution += textures2D[parameters.mRayDirect].Load( int3( int2( input.mScreenPos.xy ), 0 ) ).rgb;
    }

    // IBL Contribution
    float coatNdotV = clamp( dot( coatNormal, v ), 0.001, 1.0 );
    float NdotV = clamp( dot( n, v ), 0.001, 1.0 );
    float3 iblLighting = float3( 0.0 );

    if ( ( parameters.mPrefilterMipLevelsAndSkybox & 0x80000000u ) != 0u ) {
        iblLighting = GetIBLContribution( NdotV, diffuseColor, specularColor, perceptualRoughness, n, reflection,
            brdfLUT, irradianceMap, prefilteredMap, samplerCube, useRayLighting );
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

    // Emission
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
    // Explicit identity covers unlit/shadowed pixels too; luminance is not a mask.
    output.mColor = float4( lightContribution, float( input.mMaterialIndex + 1u ) );
    output.mSpecular = float4( sharpSpecular, 0 );
    output.mAmbient = float4( iblLighting * ambientOcclusion + emissiveContrib, 0 );
    output.mIdentityDepth = float4( -input.mFragmentViewPos.z, 0, 0, float( input.mDrawIdentity ) );
    return output;
}
