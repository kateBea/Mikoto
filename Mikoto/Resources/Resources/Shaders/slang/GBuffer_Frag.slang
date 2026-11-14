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

#include "base/Base.slang"
#include "base/SurfaceShading_Helpers.slang"
#include "base/Camera_Helpers.slang"
#include "base/Lighting_Helpers.slang"
#include "base/Material_Helpers.slang"
#include "base/Memory_Helpers.slang"

struct FSInput {
    float3 ViewPos : TEXCOORD0;
    float3 WorldPos : TEXCOORD1;
    float3 Normal : TEXCOORD2;
    float2 Uv0 : TEXCOORD3;
    float2 Uv1 : TEXCOORD4;
    float4 Color : TEXCOORD5;

    nointerpolation uint MaterialIndex : TEXTCOORD6;
    nointerpolation uint FaceFlags : TEXCOORD7;

    float4 ScreenPos : SV_Position;
};

struct FSOutput {
    float4 Position : SV_Target0;
    float4 Normal : SV_Target1;
    float4 Color : SV_Target2;
    float4 Emissive : SV_Target3;
    float4 Contour : SV_Target4;
};

struct DrawParams {
    Pointer mGeometryBufferID;
    Pointer mMaterialsBufferID;
    Pointer mSkinningBufferID;
    Pointer mGeometryAllocationBufferID;
    Pointer mCameraDataID;

    uint mBasicSamplerID;
};

MKT_DECLARE_STRUCTURED_SRV buffers;

MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;

MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );

[shader( "fragment" )]
FSOutput main( FSInput input, bool frontFacing : SV_IsFrontFace ) {
    FSOutput o;
    float3 flatNormal = SurfaceFaceNormal( ddx( input.WorldPos ), ddy( input.WorldPos ), input.Normal );
    if ( MaterialBackfaceDiscard( input.FaceFlags, frontFacing ) ) {
        discard;
    }

    BufferReference<MaterialParameters> materialsBuffer =
            ( BufferReference<MaterialParameters> )parameters.mMaterialsBufferID;
    MaterialParameters materialInfo = materialsBuffer[input.MaterialIndex];
    // Only visible opaque toon/cel pixels participate in crease contours.
    o.Contour = float4( 0 );
    if ( SurfaceStylized( uint( materialInfo.Subsurface.z ) ) && materialInfo.AlphaMask == PBR_ALPHA_OPAQUE ) {
        o.Contour = float4( materialInfo.SubsurfaceColorOpacity.rgb, saturate( materialInfo.SubsurfaceColorOpacity.a / 12.0 ) );
    }
    // SSAO and other depth consumers need the same nearest opaque surface as
    // the depth prepass, including opaque pixels authored in BLEND materials.
    if ( materialInfo.AlphaMask == PBR_ALPHA_MASK || materialInfo.AlphaMask == PBR_ALPHA_BLEND ) {
        int textureIndex = MaterialAlphaTexture( materialInfo );
        float alpha = textureIndex == INVALID_TEXTURE_INDEX
                              ? 1.0f
                              : textures[NonUniformResourceIndex( textureIndex )]
                                        .Sample( samplers[parameters.mBasicSamplerID],
                                                 MaterialAlphaUV( materialInfo, input.Uv0, input.Uv1 ) )
                                        .a;
        if ( MaterialSurfaceDiscard( materialInfo, MaterialAlpha( materialInfo, alpha, input.Color.a ), false ) ) {
            discard;
        }
    }

    input.Uv0 = input.Uv0 * materialInfo.UvTransform.xy + materialInfo.UvTransform.zw;
    input.Uv1 = input.Uv1 * materialInfo.UvTransform.xy + materialInfo.UvTransform.zw;

    CameraInfo camera = ( ( BufferReference<CameraInfo> )parameters.mCameraDataID )[0];

    // NORMAL
    float2 normalUvs = materialInfo.NormalTextureSet == 0 ? input.Uv0 : input.Uv1;
    float3 N = ( uint( materialInfo.Subsurface.z ) != SURFACE_FLAT && materialInfo.NormalIndex != INVALID_TEXTURE_INDEX )
                       ? GetNormalFromMap( textures[materialInfo.NormalIndex], samplers[parameters.mBasicSamplerID],
                                           normalUvs, input.Normal, input.WorldPos, materialInfo.NormalScale )
                       : normalize( input.Normal );

    if ( !MaterialFrontFacing( input.FaceFlags, frontFacing ) ) {
        N = -N;
    }

    if ( uint( materialInfo.Subsurface.z ) == SURFACE_FLAT ) {
        N = MaterialFrontFacing( input.FaceFlags, frontFacing ) ? flatNormal : -flatNormal;
    }
    o.Normal = float4( normalize( N ) * 0.5 + 0.5, 1.0 );

    // POSITION (view-space + depth)
    // SSAO needs distances in view-space units. Normalizing this vector
    // destroyed both depth and radius, and made occlusion camera-dependent.
    o.Position = float4( input.ViewPos, -input.ViewPos.z );

    // ALBEDO
    // Store linear base color for the G-buffer debug view.
    o.Color = float4( 0 );
    if ( materialInfo.Workflow == PBR_WORKFLOW_METALLIC_ROUGHNESS ) {
        float2 baseColorUvs = materialInfo.BaseColorTextureSet == 0 ? input.Uv0 : input.Uv1;

        float4 albedo = ( materialInfo.AlbedoIndex != INVALID_TEXTURE_INDEX )
                                ? SRGBtoLINEAR( textures[materialInfo.AlbedoIndex].Sample( samplers[parameters.mBasicSamplerID],
                                                                                  baseColorUvs ) ) * materialInfo.BaseColorFactor
                                : materialInfo.BaseColorFactor;

        o.Color = float4( albedo.xyz * input.Color.rgb, 1.0 );
    }

    if ( materialInfo.Workflow == PBR_WORKFLOW_SPECULAR_GLOSSINESS ) {
        float2 diffuseUvs = materialInfo.BaseColorTextureSet == 0 ? input.Uv0 : input.Uv1;

        float4 diffuse = ( materialInfo.DiffuseIndex != INVALID_TEXTURE_INDEX )
                                 ? SRGBtoLINEAR( textures[materialInfo.DiffuseIndex].Sample(
                                                samplers[parameters.mBasicSamplerID], diffuseUvs ) ) * materialInfo.DiffuseFactor
                                 : materialInfo.DiffuseFactor;

        o.Color = float4( diffuse.xyz * input.Color.rgb, 1.0 );
    }

    // Emisive
    o.Emissive = float4( 0, 0, 0, 0 );
    if ( materialInfo.IsBloomy == MKT_SHADER_TRUE ) {
        float3 emissiveContrib = materialInfo.EmissiveFactor.xyz * materialInfo.EmissiveStrength;

        if ( materialInfo.EmissiveIndex != INVALID_TEXTURE_INDEX ) {
            Texture2D emissiveMap = textures[materialInfo.EmissiveIndex];
            float2 emissiveUvSet = materialInfo.EmissiveTextureSet == 0 ? input.Uv0 : input.Uv1;

            float3 emissive =
                    SRGBtoLINEAR( emissiveMap.Sample( samplers[parameters.mBasicSamplerID], emissiveUvSet ) ).rgb;

            emissiveContrib *= emissive;
        }

        if ( uint( materialInfo.Subsurface.z ) == SURFACE_CLEAR_COAT ) {
            float3 view = normalize( camera.mViewPosition.xyz - input.WorldPos );
            emissiveContrib *= 1 - materialInfo.SurfaceShading.x * SurfaceCoatFresnel( dot( N, view ) );
        }
        o.Emissive = float4( emissiveContrib, 1.0f );
    }

    return o;
}
