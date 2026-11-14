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

#pragma once

#include "base/Base.slang"
#include "base/SurfaceShading_Helpers.slang"
#include "base/Camera_Helpers.slang"
#include "base/ClusteringLights_Helpers.slang"
#include "base/Lighting_Helpers.slang"
#include "base/Material_Helpers.slang"
#include "base/Math_Helpers.slang"
#include "base/Memory_Helpers.slang"
#include "base/ShadowMap_Helpers.slang"
#include "base/Tonemap_Helpers.slang"
#include "base/Atmosphere_Helpers.slang"

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

MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures2D;
MKT_DECLARE_TEXTURE_SRV( TextureCube ) texturesCube;

MKT_DECLARE_SAMPLERS samplers;

MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );

// Encapsulate the various inputs used by the various functions in the shading equation
// We store values in this struct to simplify the integration of alternative implementations
// of the shading terms, outlined in the Readme.MD Appendix.
struct PBRInfo {
    float NdotL;// cos angle between normal and light direction
    float NdotV;// cos angle between normal and view direction
    float NdotH;// cos angle between normal and half vector
    float LdotH;// cos angle between light direction and half vector
    float VdotH;// cos angle between view direction and half vector

    float perceptualRoughness;// roughness value, as authored by the model creator (input to shader)
    float metalness;          // metallic value at the surface
    float3 reflectance0;      // full reflectance color (normal incidence angle)
    float3 reflectance90;     // reflectance color at grazing angle
    float alphaRoughness;     // roughness mapped to a more linear change in the roughness (proposed by [2])
    float3 diffuseColor;      // color contribution from diffuse lighting
    float3 specularColor;     // color contribution from specular lighting
};

struct BaseInputs {
    float3 coatNormal;
    uint model;
    float4 settings;
    float bands;
    float perceptualRoughness;
    float metallic;
    float3 specularEnvironmentR0;
    float3 specularEnvironmentR90;
    float alphaRoughness;
    float3 diffuseColor;
    float3 specularColor;
};

struct FSInput {
    float3 FragmentViewPos : TEXCOORD0;
    float3 FragmentWorldPos : TEXCOORD1;
    float3 Normal : NORMAL;
    float2 TexCoord0 : TEXCOORD3;
    float2 TexCoord1 : TEXCOORD4;
    float4 VertexColor : TEXCOORD5;

    nointerpolation uint MaterialIndex : TEXCOORD6;
    nointerpolation uint FaceFlags : TEXCOORD7;
    nointerpolation uint DrawIdentity : TEXCOORD8;

    float4 ScreenPos : SV_Position;
    bool IsFrontFace : SV_IsFrontFace;
};

// Calculation of the lighting contribution from an optional Image Based Light source.
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
    return pbrInputs.diffuseColor / PI_Value;
}

// Disney Implementation of diffuse from Physically-Based Shading at Disney by Brent Burley. See Section 5.3.
// http://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
float3 DiffuseBurley( PBRInfo pbrInputs ) {
    float f90 = 2.0 * pbrInputs.LdotH * pbrInputs.LdotH * pbrInputs.alphaRoughness - 0.5;

    return ( pbrInputs.diffuseColor / PI_Value ) * ( 1.0 + f90 * pow( ( 1.0 - pbrInputs.NdotL ), 5.0 ) ) *
           ( 1.0 + f90 * pow( ( 1.0 - pbrInputs.NdotV ), 5.0 ) );
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
float3 SpecularReflection( PBRInfo pbrInputs ) {
    return pbrInputs.reflectance0 +
           ( pbrInputs.reflectance90 - pbrInputs.reflectance0 ) * pow( clamp( 1.0 - pbrInputs.VdotH, 0.0, 1.0 ), 5.0 );
}

// This calculates the specular geometric attenuation (aka G()),
// where rougher material will reflect less light back to the viewer.
float GeometricOcclusion( PBRInfo pbrInputs ) {
    float NdotL = pbrInputs.NdotL;
    float NdotV = pbrInputs.NdotV;
    float r = pbrInputs.alphaRoughness;

    float attenuationL = 2.0 * NdotL / ( NdotL + sqrt( r * r + ( 1.0 - r * r ) * ( NdotL * NdotL ) ) );
    float attenuationV = 2.0 * NdotV / ( NdotV + sqrt( r * r + ( 1.0 - r * r ) * ( NdotV * NdotV ) ) );
    return attenuationL * attenuationV;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games
float MicrofacetDistribution( PBRInfo pbrInputs ) {
    float roughnessSq = pbrInputs.alphaRoughness * pbrInputs.alphaRoughness;
    float f = ( pbrInputs.NdotH * roughnessSq - pbrInputs.NdotH ) * pbrInputs.NdotH + 1.0;
    return roughnessSq / ( PI_Value * f * f );
}

float3 ComputeDirectionalLightContribution( float3 n, float3 v, LightInfo lightInfo, BaseInputs inputs, out float3 sharpSpecular, float visibility = 1.0 ) {
    float3 l = normalize(
            -lightInfo.Direction
                     .xyz );// Vector from surface point to light (multiply by -1 because otherwise wrong face is lit)
    float3 h = normalize( l + v );// Half vector between both l and v

    float NdotL = clamp( dot( n, l ), 0.001, 1.0 );
    float NdotV = clamp( abs( dot( n, v ) ), 0.001, 1.0 );
    float NdotH = clamp( dot( n, h ), 0.0, 1.0 );
    float LdotH = clamp( dot( l, h ), 0.0, 1.0 );
    float VdotH = clamp( dot( v, h ), 0.0, 1.0 );

    PBRInfo pbrInputs = PBRInfo( NdotL, NdotV, NdotH, LdotH, VdotH, inputs.perceptualRoughness, inputs.metallic,
                                 inputs.specularEnvironmentR0, inputs.specularEnvironmentR90, inputs.alphaRoughness,
                                 inputs.diffuseColor, inputs.specularColor );

    // Calculate the shading terms for the microfacet specular shading model
    float3 F = SpecularReflection( pbrInputs );// kS
    float G = GeometricOcclusion( pbrInputs );
    float D = MicrofacetDistribution( pbrInputs );

    // Calculation of analytical lighting contribution, ks + kD = 1 with kD = 1.0 - F
    float3 diffuseContrib = ( 1.0 - F ) * DiffuseBurley( pbrInputs );
    float3 specContrib = F * G * D / ( 4.0 * NdotL * NdotV );

    // Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
    sharpSpecular = lightInfo.Diffuse.xyz * NdotL * specContrib;
    float3 color = lightInfo.Diffuse.xyz * SurfaceDirect(
        NdotL * diffuseContrib,
        inputs.diffuseColor, inputs.specularColor, inputs.perceptualRoughness,
        n, v, l, inputs.model, inputs.settings, inputs.bands, visibility, inputs.coatNormal );

    return color;
}

// Ref: Moving frostbite to PBR
// https://www.ea.com/frostbite/news/moving-frostbite-to-pb
float SmoothDistanceAtt( float d2, float invR2 ) {
    float howFar = d2 * invR2;
    float dontDie = saturate( 1.0 - howFar * howFar );
    return dontDie * dontDie;
}

float GetDistanceAtt( float3 L_unorm, float invR2 ) {
    float d2 = dot( L_unorm, L_unorm );
    float invSq = 1.0 / max( d2, 0.0001 );

    float fade = SmoothDistanceAtt( d2, invR2 );

    return invSq * fade;
}

float GetAngleAtt( float3 L, float3 lightDir, float scale, float offset ) {
    float howInsideCone = dot( lightDir, L );// are we in the cone or nah

    float cone = saturate( howInsideCone * scale + offset );
    cone *= cone;

    return cone;
}

float3 ComputePointlLightContribution( float3 n, float3 v, LightInfo lightInfo, BaseInputs inputs, float3 worldPos, out float3 sharpSpecular, float visibility = 1.0 ) {
    // Light vector and distance (v, l)
    const float3 lightVector = lightInfo.Position.xyz - worldPos;
    const float distance = length( lightVector );

    float attenuation = 1;

    const float radius = lightInfo.Radius;
    const float lightInvSqrAttRadius = 1 / ( radius * radius );

    attenuation *= GetDistanceAtt( lightVector, lightInvSqrAttRadius );

    const float intensityBaseFactor = 1.0f;
    const float3 lightIntensity = lightInfo.Diffuse.xyz * ( lightInfo.Intensity * intensityBaseFactor ) * attenuation;

    const float3 l = lightVector / max( distance, 1e-5 );
    const float3 h = normalize( l + v );

    const float NdotL = clamp( dot( n, l ), 0.001, 1.0 );
    const float NdotV = clamp( abs( dot( n, v ) ), 0.001, 1.0 );
    const float NdotH = clamp( dot( n, h ), 0.0, 1.0 );
    const float LdotH = clamp( dot( l, h ), 0.0, 1.0 );
    const float VdotH = clamp( dot( v, h ), 0.0, 1.0 );

    PBRInfo pbrInputs = PBRInfo( NdotL, NdotV, NdotH, LdotH, VdotH, inputs.perceptualRoughness, inputs.metallic,
                                 inputs.specularEnvironmentR0, inputs.specularEnvironmentR90, inputs.alphaRoughness,
                                 inputs.diffuseColor, inputs.specularColor );

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
        inputs.diffuseColor, inputs.specularColor, inputs.perceptualRoughness,
        n, v, l, inputs.model, inputs.settings, inputs.bands, visibility, inputs.coatNormal );

    return color;
}

float3 ComputeSpotLightContribution( float3 n, float3 v, LightInfo lightInfo, BaseInputs inputs, float3 worldPos, out float3 sharpSpecular, float visibility = 1.0 ) {
    float3 lightVector = lightInfo.Position.xyz - worldPos;

    float distance = length( lightVector );
    float3 l = lightVector / max( distance, 1e-5 );

    // distance attenuation (Smooth fallof not physically correct)
    float distanceAttenuation = saturate( 1.0 - distance / lightInfo.Radius );
    distanceAttenuation *= distanceAttenuation;

    // spotlight cone
    float theta = dot( normalize( -lightInfo.Direction.xyz ), l );

    float epsilon = lightInfo.CutOff - lightInfo.OuterCutOff;
    float coneAttenuation = epsilon > 1e-6 ? saturate( ( theta - lightInfo.OuterCutOff ) / epsilon )
                                           : ( theta >= lightInfo.OuterCutOff ? 1.0 : 0.0 );

    // final attenuation
    float attenuation = distanceAttenuation * coneAttenuation;

    const float intensityBaseFactor = 1000.0f;
    const float3 lightIntensity = lightInfo.Diffuse.xyz * ( lightInfo.Intensity * intensityBaseFactor ) * attenuation;

    float3 h = normalize( l + v );

    float NdotL = clamp( dot( n, l ), 0.001, 1.0 );
    float NdotV = clamp( abs( dot( n, v ) ), 0.001, 1.0 );
    float NdotH = clamp( dot( n, h ), 0.0, 1.0 );
    float LdotH = clamp( dot( l, h ), 0.0, 1.0 );
    float VdotH = clamp( dot( v, h ), 0.0, 1.0 );

    PBRInfo pbrInputs = PBRInfo( NdotL, NdotV, NdotH, LdotH, VdotH, inputs.perceptualRoughness, inputs.metallic,
                                 inputs.specularEnvironmentR0, inputs.specularEnvironmentR90, inputs.alphaRoughness,
                                 inputs.diffuseColor, inputs.specularColor );

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
        inputs.diffuseColor, inputs.specularColor, inputs.perceptualRoughness,
        n, v, l, inputs.model, inputs.settings, inputs.bands, visibility, inputs.coatNormal );

    return color;
}

struct FSOutput {
    float4 Color : SV_Target0;
    float4 Specular : SV_Target1;
    float4 Ambient : SV_Target2;
    float4 IdentityDepth : SV_Target3;
};

ShadowMetadata LoadShadowMetadata( uint recordIndex ) {
    return buffers[parameters.mShadowInfoBufferID].LoadByteOffset<ShadowMetadata>(
            recordIndex * sizeof( ShadowRecord ) + ShadowMetadataOffset );
}

float4x4 LoadShadowMatrix( uint recordIndex, uint face ) {
    return buffers[parameters.mShadowInfoBufferID].LoadByteOffset<float4x4>( recordIndex * sizeof( ShadowRecord ) +
                                                                             face * sizeof( float4x4 ) );
}

float CompareShadowTexel( ShadowMetadata record, float4x4 viewProjection, Texture2D texture, int2 dimensions,
                          int2 pixel, float3 ndc, float3 receiverWorld, float3 receiverNormal, float bias ) {
    if ( any( pixel < 0 ) || any( pixel >= dimensions ) ) {
        return 1;
    }
    float2 texelUV = ( float2( pixel ) + 0.5 ) / float2( dimensions );
    float storedDepth = texture.Load( int3( pixel, 0 ) ).r;
    float receiverDepth;
    if ( record.LightType == LIGHT_TYPE_DIRECTIONAL ) {
        receiverDepth = DirectionalShadowReceiverDepth( viewProjection, texelUV, ndc, receiverNormal );
    } else {
        receiverDepth = LocalShadowReceiverDepth( viewProjection, texelUV, receiverWorld, receiverNormal,
                                                  record.PositionRange );
    }
    if ( receiverDepth < 0.0 || receiverDepth >= 1.0 ) {
        return 1.0;
    }
    return receiverDepth - bias <= storedDepth ? 1.0 : 0.0;
}

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
    uint width, height;
    texture.GetDimensions( width, height );
    int2 pixel = int2( uv * float2( width, height ) );
    return CompareShadowTexel( record, viewProjection, texture, int2( width, height ), pixel, ndc, receiverWorld,
                               receiverNormal, bias );
}

float ShadowPCF( uint recordIndex, ShadowMetadata record, uint face, float3 world, float3 normal, float bias ) {
    // All nine taps use one face and receiver projection. Keep matrix loads,
    // descriptor lookup, dimensions and the clip test outside the filter loop.
    float4x4 viewProjection = LoadShadowMatrix( recordIndex, face );
    // Orthographic row 2 maps world-space distances along the light's
    // depth axis to normalized depth. Keep contact bias constant across
    // cascades even when their caster padding/depth spans differ.
    if ( record.LightType == LIGHT_TYPE_DIRECTIONAL ) {
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
    uint width, height;
    texture.GetDimensions( width, height );
    float2 uv = float2( ndc.x * .5 + .5, .5 - ndc.y * .5 );
    int2 pixel = int2( uv * float2( width, height ) );
    float sum = 0;
    for ( int y = -1; y <= 1; ++y ) {
        for ( int x = -1; x <= 1; ++x ) {
            sum += CompareShadowTexel( record, viewProjection, texture, int2( width, height ), pixel + int2( x, y ),
                                       ndc, world, normal, bias );
        }
    }
    return sum / 9.0;
}

float LightShadow( LightInfo light, float3 world, float3 normal, float viewDepth ) {
    if ( light.ShadowIndex < 0 || uint( light.ShadowIndex ) >= parameters.mShadowRecordCount ) {
        return 1;
    }
    uint recordIndex = uint( light.ShadowIndex );
    ShadowMetadata record = LoadShadowMetadata( recordIndex );
    if ( record.FaceCount == 0 || record.LightType != light.ActiveLightType ) {
        return 1;
    }
    float3 toLight = light.ActiveLightType == LIGHT_TYPE_DIRECTIONAL ? normalize( -light.Direction.xyz )
                                                                     : normalize( light.Position.xyz - world );
    float bias = record.Bias.x + record.Bias.y * ( 1 - saturate( abs( dot( normal, toLight ) ) ) );
    if ( light.ActiveLightType == LIGHT_TYPE_DIRECTIONAL ) {
        if ( viewDepth <= 0 || viewDepth > record.CascadeSplits.w ) {
            return 1;
        }
        uint cascade = 0;
        while ( cascade + 1 < record.FaceCount && viewDepth > record.CascadeSplits[cascade] ) {
            ++cascade;
        }
        float visibility = ShadowPCF( recordIndex, record, cascade, world, normal, bias );
        float start = cascade == 0 ? record.PositionRange.w : record.CascadeSplits[cascade - 1];
        float blend = saturate( ( viewDepth - lerp( start, record.CascadeSplits[cascade], .9 ) ) /
                                max( ( record.CascadeSplits[cascade] - start ) * .1, .0001 ) );
        if ( blend > 0 ) {
            float next = cascade + 1 < record.FaceCount
                                 ? ShadowPCF( recordIndex, record, cascade + 1, world, normal, bias )
                                 : 1;
            visibility = lerp( visibility, next, blend );
        }
        return visibility;
    }
    if ( light.ActiveLightType == LIGHT_TYPE_POINT ) {
        float3 delta = world - record.PositionRange.xyz;
        float distance = length( delta );
        if ( distance < 1e-5 || distance >= record.PositionRange.w ) {
            return 1;
        }
        float3 direction = delta / distance;
        float3 up = abs( direction.y ) < .99 ? float3( 0, 1, 0 ) : float3( 0, 0, 1 );
        float3 tangent = normalize( cross( direction, up ) ), bitangent = cross( direction, tangent );
        uint width, height;
        textures2D[NonUniformResourceIndex( record.TextureID( 0 ) )].GetDimensions( width, height );
        float sum = 0;
        // Re-select the face for every filter ray, including across cube seams.
        for ( int y = -1; y <= 1; ++y ) {
            for ( int x = -1; x <= 1; ++x ) {
                float3 ray = normalize( direction + ( tangent * x + bitangent * y ) * ( 2.0 / width ) );
                sum += ShadowTexel( recordIndex, record, PointShadowFace( ray ),
                                    record.PositionRange.xyz + ray * distance, world, normal, bias );
            }
        }
        return sum / 9.0;
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
    if ( !any( material.SubsurfaceTransmission.rgb > 0 ) ) { return float3( 0 ); }
    float3 vector = light.Position.xyz - world;
    float distance = length( vector );
    float3 l = light.ActiveLightType == LIGHT_TYPE_DIRECTIONAL ? normalize( -light.Direction.xyz ) : vector / max( distance, 1e-6 );
    float back = saturate( ( 0.3 - dot( l, normal ) ) / 1.3 );
    float3 radiance = light.Diffuse.rgb;
    if ( light.ActiveLightType == LIGHT_TYPE_POINT ) {
        radiance *= light.Intensity * GetDistanceAtt( vector, 1 / max( light.Radius * light.Radius, 1e-6 ) );
    } else if ( light.ActiveLightType == LIGHT_TYPE_SPOT ) {
        float theta = dot( normalize( -light.Direction.xyz ), l );
        float cone = saturate( ( theta - light.OuterCutOff ) / max( light.CutOff - light.OuterCutOff, 1e-6 ) );
        float fade = saturate( 1 - distance / max( light.Radius, 1e-6 ) );
        radiance *= light.Intensity * 1000 * fade * fade * cone;
    }
    return albedo * radiance * material.SubsurfaceTransmission.rgb * back *
        saturate( material.SubsurfaceColorOpacity.rgb ) * saturate( material.Subsurface.y * ( 1 - material.SubsurfaceColorOpacity.a ) );
}

[shader( "fragment" )]
FSOutput main( FSInput input ) {
    FSOutput output = (FSOutput)0;

    BufferReference<MaterialParameters> materialsBuffer = ( BufferReference<MaterialParameters> )parameters.mMaterialsPtr;
    MaterialParameters material = materialsBuffer[input.MaterialIndex];
    input.TexCoord0 = input.TexCoord0 * material.UvTransform.xy + material.UvTransform.zw;
    input.TexCoord1 = input.TexCoord1 * material.UvTransform.xy + material.UvTransform.zw;

    // ddx measures how UVs change horizontally across screen pixels; ddy measures
    // the vertical change. These gradients guide mip selection and texture filtering.
    // Sample normally computes them implicitly. Save them before discard and branches,
    // then pass them to SampleGrad later so those samples do not need to calculate
    // derivatives inside control flow where neighboring pixels may take different paths.
    // This lets metallic/roughness and AO reads happen after alpha rejection, skipping
    // those texture reads for discarded fragments.
    float2 uv0Dx = ddx( input.TexCoord0 ), uv0Dy = ddy( input.TexCoord0 );
    float2 uv1Dx = ddx( input.TexCoord1 ), uv1Dy = ddy( input.TexCoord1 );

    // Evaluate derivatives before discard and divergent light loops. Normal
    // maps and interpolated vertex normals do not describe the rasterized plane.
    float3 receiverNormal = cross( ddx( input.FragmentWorldPos ), ddy( input.FragmentWorldPos ) );
    float receiverNormalLength = length( receiverNormal );
    receiverNormal = receiverNormalLength > 1e-12 ? receiverNormal / receiverNormalLength : normalize( input.Normal );

    float3 flatNormal = SurfaceFaceNormal( ddx( input.FragmentWorldPos ), ddy( input.FragmentWorldPos ), input.Normal );
    if ( MaterialBackfaceDiscard( input.FaceFlags, input.IsFrontFace ) ) {
        discard;
    }

    uint shadingModel = uint( material.Subsurface.z );
    // Ray lighting currently evaluates only the base PBR BRDF. These models
    // retain raster lights/shadows and IBL instead of silently losing their lobes.
    bool useRayLighting = !SurfaceRasterModel( shadingModel ) && shadingModel != 5;
    CameraInfo camera = ( ( BufferReference<CameraInfo> )parameters.mCameraPtr )[0];
    if ( material.Subsurface.z != 5.0 || material.Subsurface.x <= 0.0 || material.Subsurface.y <= 0.0 ||
         material.AlphaMask == PBR_ALPHA_BLEND || material.Workflow == PBR_WORKFLOW_UNLIT || camera.mVisualizeCascades != 0 ||
         ( ( input.FaceFlags >> 2 ) & 3u ) != 0 ) {
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

    if ( material.Workflow == PBR_WORKFLOW_METALLIC_ROUGHNESS ) {
        // Metallic and Roughness material properties are packed together
        // In glTF, these factors can be specified by fixed scalar values
        // or from a metallic-roughness map
        perceptualRoughness = material.RoughnessFactor;
        metallic = material.MetallicFactor;

        // The albedo may be defined from a base texture or a flat color
        if ( material.AlbedoIndex != INVALID_TEXTURE_INDEX ) {
            Texture2D colorMap = textures2D[NonUniformResourceIndex( material.AlbedoIndex )];
            baseColor = SRGBtoLINEAR( colorMap.Sample(samplerBasic, material.BaseColorTextureSet == 0 ? input.TexCoord0 : input.TexCoord1 ) ) * material.BaseColorFactor;
        } else {
            baseColor = material.BaseColorFactor;
        }
    }

    if ( material.Workflow == PBR_WORKFLOW_SPECULAR_GLOSSINESS ) {
        // RGB and alpha belong to one texel; retain the raw alpha while
        // decoding only colour channels for the specular/glossiness workflow.
        float4 specGloss = float4( 1.0 );
        if ( material.SpecularGlossinessIndex != INVALID_TEXTURE_INDEX ) {
            specGloss = textures2D[NonUniformResourceIndex( material.SpecularGlossinessIndex )].Sample(
                samplerBasic, material.SpecularGlossinessSet == 0 ? input.TexCoord0 : input.TexCoord1 );
        }
        perceptualRoughness = 1.0 - specGloss.a;
        const float epsilon = 1e-6;
        float4 diffuse = material.DiffuseIndex != INVALID_TEXTURE_INDEX
            ? SRGBtoLINEAR( textures2D[NonUniformResourceIndex( material.DiffuseIndex )].Sample(
                samplerBasic, ( material.BaseColorTextureSet == 0 ? input.TexCoord0 : input.TexCoord1 ) ) )
            : float4( 1.0 );
        float3 specular = SRGBtoLINEAR( specGloss ).rgb;

        float maxSpecular = max( max( specular.r, specular.g ), specular.b );

        // Convert metallic value from specular glossiness inputs
        metallic = ConvertMetallic( diffuse.rgb, specular, maxSpecular );

        float3 baseColorDiffusePart = diffuse.rgb *
                                      ( ( 1.0 - maxSpecular ) / ( 1 - MIN_ROUGHNESS ) / max( 1 - metallic, epsilon ) ) *
                                      material.DiffuseFactor.rgb;
        float3 baseColorSpecularPart =
                specular - ( float3( MIN_ROUGHNESS ) * ( 1 - metallic ) * ( 1 / max( metallic, epsilon ) ) ) *
                                   material.SpecularFactor.rgb;
        baseColor = float4( lerp( baseColorDiffusePart, baseColorSpecularPart, metallic * metallic ),
                            diffuse.a * material.DiffuseFactor.a );
    }

    baseColor *= float4( input.VertexColor );
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
    if ( material.Workflow == PBR_WORKFLOW_METALLIC_ROUGHNESS ) {
        if ( material.MetallicRoughnessIndex != INVALID_TEXTURE_INDEX ) {
            // Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
            // This layout intentionally reserves the 'r' channel for (optional) occlusion map data
            Texture2D metallicRoughnessMap = textures2D[NonUniformResourceIndex( material.MetallicRoughnessIndex )];
            mrSample = metallicRoughnessMap.SampleGrad(
                    samplerBasic, material.MetallicRoughnessTextureSet == 0 ? input.TexCoord0 : input.TexCoord1,
                    material.MetallicRoughnessTextureSet == 0 ? uv0Dx : uv1Dx,
                    material.MetallicRoughnessTextureSet == 0 ? uv0Dy : uv1Dy );

            perceptualRoughness = mrSample.g * perceptualRoughness;
            metallic = mrSample.b * metallic;
        } else {
            perceptualRoughness = clamp( perceptualRoughness, MIN_ROUGHNESS, 1.0 );
            metallic = clamp( metallic, 0.0, 1.0 );
        }
    }
    // Opaque and surviving cutout fragments fully cover their pixel. Only BLEND
    // materials pass fractional opacity to the transparent pipeline.
    if ( material.AlphaMask != PBR_ALPHA_BLEND ) {
        baseColor.a = 1.0f;
    }

    uint cullingClass = ( input.FaceFlags >> 2 ) & 3u;
    if ( cullingClass != 0 ) {
        float3 tint = cullingClass == 1   ? float3( 0.3, 0.9, 0.06 )
                      : cullingClass == 2 ? float3( 0.9, 0.04, 0.025 )
                                          : float3( 1.0, 0.55, 0.04 );
        // Preserve material cutouts, sidedness and transparent opacity in debug mode.
        output.Color = float4( tint, baseColor.a );
        return output;
    }

    // dielectrics default to 4%
    float3 f0 = float3( 0.04 );
    float transmission = 0;
    if ( shadingModel == SURFACE_DIELECTRIC ) {
        float ior = max( material.SurfaceShading.x, 1.0 );
        float ratio = (ior - 1) / (ior + 1);
        f0 = float3( ratio * ratio );
        metallic = 0;
        transmission = saturate( material.SurfaceShading.y );
    }

    // diffuse should disappear as the material becomes metallic.
    float diffuseFactor = (1.0f - metallic) * (1 - transmission);

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
    if ( shadingModel == SURFACE_DIELECTRIC ) { reflectance90 = 1.0; }
    float3 specularEnvironmentR0 = specularColor.rgb;
    float3 specularEnvironmentR90 = float3( 1.0, 1.0, 1.0 ) * reflectance90;

    float2 normalUvSet = material.NormalTextureSet == 0 ? input.TexCoord0 : input.TexCoord1;
    float3 n = ( shadingModel != SURFACE_FLAT && material.NormalIndex != INVALID_UV_SET )
                       ? GetNormalFromMap( textures2D[NonUniformResourceIndex( material.NormalIndex )], samplerBasic,
                                           normalUvSet, input.Normal, input.FragmentWorldPos, material.NormalScale )
                       : normalize( input.Normal );

    float3 coatNormal = normalize( input.Normal );
    if ( shadingModel == SURFACE_CLEAR_COAT && material.ClearCoatNormalIndex >= 0 ) {
        float2 coatUv = material.ClearCoatNormalTextureSet == 0 ? input.TexCoord0 : input.TexCoord1;
        coatNormal = GetClearCoatNormalFromMap( textures2D[NonUniformResourceIndex( material.ClearCoatNormalIndex )],
            samplerBasic, coatUv, input.Normal, input.FragmentWorldPos, material.ClearCoatNormalScale );
    }
    if ( shadingModel == SURFACE_FLAT ) { n = flatNormal; }
    if ( !MaterialFrontFacing( input.FaceFlags, input.IsFrontFace ) ) {
        n = -n;
        coatNormal = -coatNormal;
    }

    float3 v = normalize( camera.mViewPosition.xyz - input.FragmentWorldPos );// Vector from surface point to camera
    float3 reflection = normalize( reflect( -v, n ) );

    BaseInputs inputs;
    inputs.coatNormal = coatNormal;
    inputs.model = shadingModel;
    inputs.settings = material.SurfaceShading;
    inputs.bands = material.Subsurface.w;
    inputs.perceptualRoughness = perceptualRoughness;
    inputs.metallic = metallic;
    inputs.specularEnvironmentR0 = specularEnvironmentR0;
    inputs.specularEnvironmentR90 = specularEnvironmentR90;
    inputs.alphaRoughness = alphaRoughness;
    inputs.diffuseColor = diffuseColor;
    inputs.specularColor = specularColor;

    float3 lightContribution = float3( 0.0 );
    float3 sharpSpecular = float3( 0.0 );

    // Cluster resolve
    // TODO: Check first if there are active lights
    BufferReference<Cluster> clusterBuffer = ( BufferReference<Cluster> )parameters.mClusterPtr;
    BufferReference<LightInfo> lightCullingBuffer = ( BufferReference<LightInfo> )parameters.mLightsPtr;

    float3 gridSize = float3( parameters.mGridSize1.xy, parameters.mGridSizeZ );

    uint clusterIndex = ComputeTileIndex( input.FragmentViewPos, gridSize, camera.mPlanes.xy, input.ScreenPos,
                                          camera.mScreenDimensions.xy );

    if ( ( !useRayLighting || parameters.mRayDirect == 0xffffffffu ) && clusterIndex < parameters.mClusterBufferSize ) {
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
            if ( light.Direction.w > 0.5 && camera.mAtmosphere != 0 ) {
                AtmosphereData atmosphere = ( ( BufferReference<AtmosphereData> )camera.mAtmosphere )[0];
                light.Diffuse.xyz = AtmosSurfaceSun( atmosphere, input.FragmentWorldPos,
                    textures2D[camera.mAtmosphereTransmittance], samplers[camera.mAtmosphereSampler] );
            }

            float3 contribution = float3( 0 );
            float3 lightSpecular = float3( 0 );

            bool stylized = SurfaceStylized( shadingModel );

            float visibility = stylized ? LightShadow( light, input.FragmentWorldPos, receiverNormal, -input.FragmentViewPos.z ) : 1.0;
            if ( light.ActiveLightType == LIGHT_TYPE_DIRECTIONAL ) {
                contribution = ComputeDirectionalLightContribution( n, v, light, inputs, lightSpecular, visibility );
            } else if ( light.ActiveLightType == LIGHT_TYPE_POINT ) {
                contribution = ComputePointlLightContribution( n, v, light, inputs, input.FragmentWorldPos, lightSpecular, visibility );
            } else if ( light.ActiveLightType == LIGHT_TYPE_SPOT ) {
                contribution = ComputeSpotLightContribution( n, v, light, inputs, input.FragmentWorldPos, lightSpecular, visibility );
            }

            lightContribution += SubsurfaceTransmission( light, material, diffuseAlbedo, input.FragmentWorldPos, n );

            // Range, cone and surface-facing tests can eliminate the light.
            // Do not fetch its PCF taps when visibility cannot affect the result.
            if ( any( contribution != float3( 0.0 ) ) || any( lightSpecular != float3( 0.0 ) ) ) {
                float shadow = stylized ? 1.0 : LightShadow( light, input.FragmentWorldPos, receiverNormal, -input.FragmentViewPos.z );
                lightContribution += contribution * shadow;
                sharpSpecular += lightSpecular * shadow;
            }
        }
    }

    if ( useRayLighting && parameters.mRayDirect != 0xffffffffu ) {
        lightContribution = textures2D[parameters.mRayDirect].Load( int3( int2( input.ScreenPos.xy ), 0 ) ).rgb;
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
        } else if ( shadingModel == SURFACE_CLEAR_COAT && material.SurfaceShading.x > 0 ) {
            float weight = material.SurfaceShading.x;
            float f = SurfaceCoatFresnel( coatNdotV );
            float roughness = material.SurfaceShading.y;
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
            uint width, height;
            textures2D[parameters.mSsaoTextureID].GetDimensions( width, height );
            int2 pixel = clamp( int2( input.ScreenPos.xy ), int2( 0 ), int2( width, height ) - 1 );
            ambientOcclusion = pow( saturate( textures2D[parameters.mSsaoTextureID].Load( int3( pixel, 0 ) ).r ),
                                    max( parameters.mSsaoIntensity, 0.0 ) );
        }

        if ( material.AoIndex != INVALID_TEXTURE_INDEX ) {
            float2 aoUV = material.OcclusionTextureSet == 0 ? input.TexCoord0 : input.TexCoord1;
            bool sharedOrm = material.Workflow == PBR_WORKFLOW_METALLIC_ROUGHNESS &&
                material.AoIndex == material.MetallicRoughnessIndex &&
                material.OcclusionTextureSet == material.MetallicRoughnessTextureSet;
            if ( sharedOrm ) {
                ambientOcclusion *= mrSample.r;
            } else {
                ambientOcclusion *= textures2D[NonUniformResourceIndex( material.AoIndex )].SampleGrad(
                    samplerBasic, aoUV, material.OcclusionTextureSet == 0 ? uv0Dx : uv1Dx,
                    material.OcclusionTextureSet == 0 ? uv0Dy : uv1Dy ).r;
            }
        }

    }

    // Emission
    float3 emissiveContrib = material.EmissiveFactor.xyz * material.EmissiveStrength;
    if ( material.EmissiveIndex != INVALID_TEXTURE_INDEX ) {
        Texture2D emissiveMap = textures2D[NonUniformResourceIndex( material.EmissiveIndex )];
        float2 emissiveUvSet = material.EmissiveTextureSet == 0 ? input.TexCoord0 : input.TexCoord1;

        // Emissive textures are sRGB, just as in the bloom/G-buffer pass.
        float3 emissive = SRGBtoLINEAR( emissiveMap.Sample( samplerBasic, emissiveUvSet ) ).rgb;

        emissiveContrib *= emissive;
    }

    if ( shadingModel == SURFACE_CLEAR_COAT ) {
        emissiveContrib *= 1 - material.SurfaceShading.x * SurfaceCoatFresnel( coatNdotV );
    }
    // Explicit identity covers unlit/shadowed pixels too; luminance is not a mask.
    output.Color = float4( lightContribution, float( input.MaterialIndex + 1u ) );
    output.Specular = float4( sharpSpecular, 0 );
    output.Ambient = float4( iblLighting * ambientOcclusion + emissiveContrib, 0 );
    output.IdentityDepth = float4( -input.FragmentViewPos.z, 0, 0, float( input.DrawIdentity ) );
    return output;
}
