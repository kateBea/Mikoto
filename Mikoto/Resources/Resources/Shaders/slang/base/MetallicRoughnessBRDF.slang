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

// Diffuse and microfacet BRDF terms for metallic-roughness shading. Fresnel, geometric masking
// and normal distribution are evaluated separately before direct-light composition.

#pragma once

#include "Math_Helpers.slang"

// Shared BRDF terms from PBR_MetallicRoughness_Frag.slang.
// https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/glTF-WebGL-PBR/shaders/pbr-frag.glsl
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
