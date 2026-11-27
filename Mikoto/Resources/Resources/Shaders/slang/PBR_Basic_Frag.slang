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

#include "base/Animation_Helpers.slang"
#include "base/Base.slang"
#include "base/Camera_Helpers.slang"
#include "base/ClusteringLights_Helpers.slang"
#include "base/Lighting_Helpers.slang"
#include "base/Material_Helpers.slang"
#include "base/Tonemap_Helpers.slang"

struct FinalShadingParameters {
    float4 mGridSize;
    float mExposure;
    float mGamma;
    float mMaxReflectionLOD;

    int mEnableSSAO;
    int mUseBlurred;
    float mSSAOIntensity;

    int mPrefilteredCubeMipLevels;
    float mScaleIBLAmbient;
    int mStep;// for debug

    int mIsSkyboxActive;
};

[[vk::binding( Slot_0, BUFFER_VIEWS_SET_INDEX )]]
ConstantBuffer<CameraInfo, ScalarDataLayout> u_Camera;

[[vk::binding( Slot_0, UNBOUNDED_IV_SAMPLERS_SET_INDEX )]]
Sampler2D g_BindlessTextures[];

[[vk::binding( Slot_3, UAV_SET_INDEX )]] StructuredBuffer<Cluster, ScalarDataLayout> Clusters;
[[vk::binding( Slot_4, UAV_SET_INDEX )]] StructuredBuffer<LightInfo, ScalarDataLayout> Lights;

[[vk::binding( Slot_1, UAV_SET_INDEX )]] StructuredBuffer<MaterialParameters, ScalarDataLayout> Materials;

[[vk::binding( Slot_0, STATIC_SAMPLERS_SET_INDEX )]] Sampler2D u_SamplerBRDFLUT;
[[vk::binding( Slot_1, STATIC_SAMPLERS_SET_INDEX )]] SamplerCube u_PrefilteredMap;
[[vk::binding( Slot_2, STATIC_SAMPLERS_SET_INDEX )]] SamplerCube u_SamplerIrradiance;

[[vk::binding( Slot_3, STATIC_SAMPLERS_SET_INDEX )]] Sampler2D u_SSAO;
[[vk::binding( Slot_4, STATIC_SAMPLERS_SET_INDEX )]] Sampler2D u_SSAOBlur;

[[vk::push_constant]]
FinalShadingParameters u_Constants;

struct FSInput {
    float3 mFragmentViewPos : TEXCOORD0;
    float3 mFragmentWorldPos : TEXCOORD1;
    float3 mNormal : TEXCOORD2;
    float2 mTexCoord : TEXCOORD3;
    float3 mVertexColor : TEXCOORD4;
    float2 mTexCoord1 : TEXCOORD5;
    int mMaterialIndex : TEXCOORD5;

    float4 mScreenPos : SV_Position;
};

struct FSOutput {
    float4 mColor : SV_Target0;
};

[shader( "fragment" )]
FSOutput main( FSInput input ) {
    FSOutput o;

    MaterialParameters material = Materials[input.mMaterialIndex];

    float4 albedo = ( material.mAlbedoIndex != INVALID_TEXTURE_INDEX )
                            ? pow( g_BindlessTextures[material.mAlbedoIndex].Sample( input.mTexCoord ), float4( 2.2 ) )
                            : material.mBaseColorFactor;

    float metallic = ( material.mMetallicIndex != INVALID_TEXTURE_INDEX )
                             ? g_BindlessTextures[material.mMetallicIndex].Sample( input.mTexCoord ).r
                             : material.mMetallicFactor;

    float roughness = ( material.mRoughnessIndex != INVALID_TEXTURE_INDEX )
                              ? g_BindlessTextures[material.mRoughnessIndex].Sample( input.mTexCoord ).r
                              : material.mRoughnessFactor;

    float3 N = ( material.mNormalIndex != INVALID_TEXTURE_INDEX )
                       ? GetNormalFromMap( g_BindlessTextures[material.mNormalIndex], input.mTexCoord,
                                           input.mFragmentWorldPos, input.mNormal )
                       : normalize( input.mNormal );

    float3 V = normalize( u_Camera.ViewPosition.xyz - input.mFragmentWorldPos );
    float3 F0 = lerp( float3( 0.04 ), albedo.xyz, metallic );

    uint zTile = uint( ( log( abs( input.mFragmentViewPos.z ) / u_Camera.Planes.y ) * u_Constants.mGridSize.z ) /
                       log( u_Camera.Planes.x / u_Camera.Planes.y ) );

    float2 tileSize = u_Camera.ScreenDimensions / u_Constants.mGridSize.xy;

    uint xTile = uint( input.mScreenPos.x / tileSize.x );
    uint yTile = uint( input.mScreenPos.y / tileSize.y );

    uint tileIndex = xTile + yTile * uint( u_Constants.mGridSize.x ) +
                     zTile * uint( u_Constants.mGridSize.x * u_Constants.mGridSize.y );

    float3 Lo = float3( 0, 0, 0 );

    if ( tileIndex < Clusters.getCount() ) {
        uint lightCount = Clusters[tileIndex].mCount;

        for ( uint i = 0; i < lightCount; i++ ) {
            uint idx = Clusters[tileIndex].LightIndices[i];
            LightInfo L = Lights[idx];

            switch ( L.mActiveLightType ) {
                case LIGHT_TYPE_POINT:
                    Lo += ComputePointLight( N, V, F0, roughness, metallic, albedo.xyz, L, input.mFragmentWorldPos );
                    break;

                case LIGHT_TYPE_SPOT:
                    Lo += ComputeSpotLight( N, V, F0, roughness, metallic, albedo.xyz, L, input.mFragmentWorldPos );
                    break;

                case LIGHT_TYPE_DIRECTIONAL:
                    Lo += ComputeDirectionalLight( N, V, F0, roughness, metallic, albedo.xyz, L );
                    break;
            }
        }
    }

    float3 R = reflect( -V, N );

    float2 brdf = u_SamplerBRDFLUT.Sample( float2( max( dot( N, V ), 0.0 ), roughness ) ).rg;

    float3 reflection = float3( 0, 0, 0 );
    float3 irradiance = float3( 0, 0, 0 );

    float3 F = F_SchlickR( max( dot( N, V ), 0.0 ), F0, roughness );

    if ( u_Constants.mIsSkyboxActive == 1 ) {
        reflection = PrefilteredReflection( u_PrefilteredMap, R, roughness, u_Constants.mMaxReflectionLOD );
        irradiance = u_SamplerIrradiance.Sample( N ).rgb;
    }

    // SSAO
    float ssao = 1.0f;

    if ( u_Constants.mEnableSSAO != MKT_SHADER_FALSE ) {
        float2 uv = input.mScreenPos.xy / u_Camera.ScreenDimensions;

        ssao = ( material.mAoIndex != INVALID_TEXTURE_INDEX )
                       ? g_BindlessTextures[material.mAoIndex].Sample( input.mTexCoord ).r
                       : ( u_Constants.mUseBlurred == MKT_SHADER_TRUE ? u_SSAOBlur.Sample( uv ).r
                                                                     : u_SSAO.Sample( uv ).r );

        float ssaoStrength = u_Constants.mSSAOIntensity;
        ssao = pow( ssao, ssaoStrength );
    }

    float3 diffuse = irradiance * albedo.xyz;
    float3 specular = reflection * ( F * brdf.x + brdf.y );

    float3 kD = ( 1.0 - F ) * ( 1.0 - metallic );
    float3 ambient = ( kD * diffuse + specular ) * float3( ssao, ssao, ssao );

    float3 color = ambient + Lo;

    // Tone Mapping
    color = Uncharted2Tonemap( color * u_Constants.mExposure );
    color *= ( 1.0 / Uncharted2Tonemap( float3( 11.2 ) ) );

    // Gamma Correction
    color = pow( color, float3( 1.0 / u_Constants.mGamma ) );

    // Emissive
    float3 emissive = float3( 0, 0, 0 );
    if ( material.mEmissiveIndex != INVALID_TEXTURE_INDEX ) {
        Sampler2D emissiveMap = g_BindlessTextures[material.mEmissiveIndex];
        emissive = CalculateEmissive( material.mEmissiveFactor.xyz, material.mEmissiveStrength, material.mEmissiveIndex,
                                      emissiveMap, input.mTexCoord );
    } else {
        emissive = CalculateEmissive( material.mEmissiveFactor.xyz, material.mEmissiveStrength, material.mEmissiveIndex,
                                      input.mTexCoord );
    }

    float alpha = material.mAlphaMaskCutoff * albedo.a;
    if ( material.mAlbedoIndex == INVALID_TEXTURE_INDEX ) {
        alpha = material.mAlphaMaskCutoff;
    }

    o.mColor = float4( color + emissive, alpha );
    return o;
}