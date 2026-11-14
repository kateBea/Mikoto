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

#include "CompatibilityEnvironment.slang"

[[vk::binding( 17, 0 )]] Texture2D environmentAtlas : register( t17 );
[[vk::binding( 18, 0 )]] Texture2D environmentBRDF : register( t18 );

float3 ReadFilteredEnvironment( float2 uv, uint layer, uint2 tileSize ) {
    uint stride = tileSize.y + 2u * kEnvironmentGutter;
    uint2 extent = uint2( tileSize.x + 2u * kEnvironmentGutter, stride * ( kEnvironmentSpecularLevels + 1u ) );
    float2 origin = float2( kEnvironmentGutter, layer * stride + kEnvironmentGutter );
    return environmentAtlas.SampleLevel( environmentSampler, ( origin + uv * float2( tileSize ) ) / float2( extent ), 0.0 ).rgb;
}

float3 GetCompatibilityIBL( float3 normal, float3 reflection, float nDotV, float roughness,
    float3 diffuse, float3 specular, float source ) {
    uint width, height;
    environmentAtlas.GetDimensions( width, height );
    uint2 tileSize = uint2( width, height / ( kEnvironmentSpecularLevels + 1u ) ) - 2u * kEnvironmentGutter;
    float3 irradiance = ReadFilteredEnvironment( EnvironmentUV( normal ), kEnvironmentDiffuseLayer, tileSize );

    float level = roughness * float( kEnvironmentSpecularLevels );
    uint lower = uint( floor( level ) );
    uint upper = min( lower + 1u, kEnvironmentSpecularLevels );
    float2 reflectionUV = EnvironmentUV( reflection );
    float3 first = lower == 0u
        ? ReadEnvironment( reflection, 0.0, source )
        : ReadFilteredEnvironment( reflectionUV, lower, tileSize );
    float3 second = ReadFilteredEnvironment( reflectionUV, upper, tileSize );
    float3 filtered = lerp( first, second, frac( level ) );

    // Same scale/bias lookup as GetIBLContribution in PBR_MetallicRoughness_Frag.slang.
    // https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf
#if defined( MKT_SUPPORT_OPENGL )
    float2 lutUV = float2( nDotV, roughness );
#else
    float2 lutUV = float2( nDotV, 1.0 - roughness );
#endif
    float2 brdf = environmentBRDF.SampleLevel( environmentSampler, lutUV, 0.0 ).rg;
    return irradiance * diffuse + filtered * ( specular * brdf.x + brdf.y );
}
