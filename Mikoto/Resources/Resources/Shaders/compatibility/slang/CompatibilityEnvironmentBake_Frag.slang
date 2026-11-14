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

#include "base/CompatibilityEnvironment.slang"
#include "../../slang/base/Lighting_Helpers.slang"

struct BakeParameters {
    uint2 TileSize;
    float Source;
    float LastSourceMip;
};

[[vk::binding( 0, 0 )]] ConstantBuffer<BakeParameters> bake : register( b0 );

static const uint kEnvironmentSampleCount = 128u;

// Cosine-weighted sampling of the diffuse irradiance used by Irradiance_Frag.slang.
// GGX importance sampling follows PrefilterEnvMap in CubemapPrefilter_Helpers.slang.
// https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf
[shader( "fragment" )]
float4 main( float4 position : SV_Position ) : SV_Target0 {
    uint stride = bake.TileSize.y + 2u * kEnvironmentGutter;
    uint layer = uint( position.y ) / stride;
    float2 uv = ( position.xy - float2( kEnvironmentGutter, layer * stride + kEnvironmentGutter ) ) / float2( bake.TileSize );
    float3 direction = EnvironmentDirection( float2( frac( uv.x ), saturate( uv.y ) ) );

    float3 up = abs( direction.z ) < 0.999 ? float3( 0, 0, 1 ) : float3( 1, 0, 0 );
    float3 tangent = normalize( cross( up, direction ) );
    float3 bitangent = cross( direction, tangent );
    float roughness = float( layer ) / float( kEnvironmentSpecularLevels );

    float3 radiance = 0.0;
    float totalWeight = 0.0;

    for ( uint i = 0u; i < kEnvironmentSampleCount; ++i ) {
        float2 samplePoint = Hammersley2D( i, kEnvironmentSampleCount );
        float3 incoming;
        float weight;

        if ( layer == kEnvironmentDiffuseLayer ) {
            float phi = 2.0 * PI_Value * samplePoint.x;
            float radius = sqrt( samplePoint.y );
            incoming = radius * ( cos( phi ) * tangent + sin( phi ) * bitangent ) + sqrt( 1.0 - samplePoint.y ) * direction;
            weight = 1.0;
        }
        else {
            float3 halfVector = ImportanceSample_GGX( samplePoint, roughness, direction );
            incoming = reflect( -direction, halfVector );
            weight = saturate( dot( direction, incoming ) );
        }

        if ( weight > 0.0 ) {
            radiance += ReadEnvironment( incoming, roughness * bake.LastSourceMip, bake.Source ) * weight;
            totalWeight += weight;
        }
    }

    return EncodeSceneColor( radiance / max( totalWeight, 1e-6 ) );
}
