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

#include "CompatibilityFrame.slang"

#include "../../../slang/base/MetallicRoughnessBRDF.slang"

static const float kEpsilon = 0.0001;
static const float kMinimumRoughness = 0.04;
static const float kGrazingReflectanceScale = 25.0;

float3 DirectLight( Light light, float3 position, float3 normal, float3 view, float3 diffuse, float3 specular,
    float metallic, float roughness ) {
    float3 incoming = -light.DirectionRadius.xyz;
    float attenuation = 1.0;

    if ( light.PositionType.w > 0.0 ) {
        float3 offset = light.PositionType.xyz - position;
        float distanceSquared = max( dot( offset, offset ), kEpsilon );
        float radius = max( light.DirectionRadius.w, kEpsilon );
        float relativeDistance = distanceSquared / ( radius * radius );
        float fade = saturate( 1.0 - relativeDistance * relativeDistance );

        incoming = offset * rsqrt( distanceSquared );
        attenuation = fade * fade / distanceSquared;

        if ( light.PositionType.w == 2.0 ) {
            attenuation *= saturate( ( dot( -incoming, light.DirectionRadius.xyz ) - light.Spot.y ) /
                max( light.Spot.x - light.Spot.y, kEpsilon ) );
        }
    }

    float nDotL = saturate( dot( normal, incoming ) );
    if ( nDotL == 0.0 ) {
        return 0.0;
    }

    float3 sum = incoming + view;
    float3 halfVector = sum * rsqrt( max( dot( sum, sum ), kEpsilon ) );
    float nDotV = max( abs( dot( normal, view ) ), kEpsilon );
    float reflectance90 = saturate( max( max( specular.r, specular.g ), specular.b ) * kGrazingReflectanceScale );

    PBRInfo inputs;
    inputs.NdotL = nDotL;
    inputs.NdotV = nDotV;
    inputs.NdotH = saturate( dot( normal, halfVector ) );
    inputs.LdotH = saturate( dot( incoming, halfVector ) );
    inputs.VdotH = saturate( dot( view, halfVector ) );
    inputs.perceptualRoughness = roughness;
    inputs.metalness = metallic;
    inputs.reflectance0 = specular;
    inputs.reflectance90 = float3( reflectance90 );
    inputs.alphaRoughness = roughness * roughness;
    inputs.diffuseColor = diffuse;
    inputs.specularColor = specular;

    float3 fresnel = SpecularReflection( inputs );
    float3 diffuseBRDF = ( 1.0 - fresnel ) * DiffuseBurley( inputs );
    float3 specularBRDF = fresnel * GeometricOcclusion( inputs ) * MicrofacetDistribution( inputs )
        / ( 4.0 * nDotL * nDotV );
    return ( diffuseBRDF + specularBRDF ) * light.Radiance.rgb * ( attenuation * nDotL );
}
