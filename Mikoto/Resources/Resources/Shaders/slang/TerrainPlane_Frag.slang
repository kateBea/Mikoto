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

// Blend terrain material layers using paint weights and height/triplanar mapping. Evaluate
// sun/ambient lighting and compose atmospheric media over the resulting surface.

#include "base/Atmosphere_Helpers.slang"
#include "base/Camera_Helpers.slang"
#include "base/Clouds_Helpers.slang"
#include "base/Lighting_Helpers.slang"
#include "base/ShadowMap_Helpers.slang"
#include "base/TerrainLighting_Helpers.slang"
#include "base/TerrainRender_Helpers.slang"
#include "base/TerrainSurface_Helpers.slang"

float3 TerrainLinearColor( float3 color ) {
    return select( color <= .04045, color / 12.92, pow( ( color + .055 ) / 1.055, 2.4 ) );
}

// Blend samples projected along the three dominant axes. Normal-derived weights reduce UV
// stretching on steep terrain faces.
float4 TerrainTriplanar( uint index, float3 world, float3 normal, float2 scale, SamplerState sourceSampler ) {
    float3 weights = pow( abs( normal ), 4 );
    weights /= max( dot( weights, 1 ), 1e-5 );
    // Implicit derivatives choose mip levels; steep slopes use side projections.
    Texture2D<float4> source = terrainTextures[NonUniformResourceIndex( index )];
    return source.Sample( sourceSampler, world.zy * scale ) * weights.x +
           source.Sample( sourceSampler, world.xz * scale ) * weights.y +
           source.Sample( sourceSampler, world.xy * scale ) * weights.z;
}

// Weight/height blending follows the landscape material model, without relying
// on an editor-specific material graph or projecting UVs down steep slopes.
// https://dev.epicgames.com/documentation/unreal-engine/landscape-materials-in-unreal-engine
[shader( "fragment" )]
float4 main( float height : TEXCOORD0, float2 grid : TEXCOORD1, float3 world : TEXCOORD2, float3 normal : TEXCOORD3, float2 uv : TEXCOORD4, float4 position : SV_Position ) : SV_Target0 {
    TerrainSurfaceData surface = LoadBufferValue<TerrainSurfaceData>( parameters.mSurface, 0 );
    CameraInfo camera = LoadBufferValue<CameraInfo>( parameters.mCamera, 0 );
    float3 n = normalize( normal );
    float3 v = normalize( camera.mViewPosition.xyz - world );
    SamplerState layerSampler = terrainSamplers[surface.mControls.x];
    float4 weights = TerrainWeights( surface, uv, height, n.y );

    // First gather per-layer albedo and height detail using each layer mapping. The following
    // weight normalization determines which authored terrain materials contribute to this
    // pixel.
    float4 colors[kTerrainMaterialLayerCount];
    float4 detailHeights = .5;
    for ( uint layer = 0; layer < kTerrainMaterialLayerCount; ++layer ) {
        TerrainLayerData material = surface.mLayers[layer];
        colors[layer] = float4( material.mTintRoughness.rgb, 1 );
        if ( material.mTextures.x != 0xffffffffu ) {
            float3 sampled = TerrainTriplanar( material.mTextures.x, world, n, material.mUVNormalSlope.xy, layerSampler ).rgb;
            colors[layer].rgb *= ( material.mTextures.w & 1u ) != 0 ? TerrainLinearColor( sampled ) : sampled;
        }
        if ( material.mTextures.z != 0xffffffffu ) {
            detailHeights[layer] = TerrainTriplanar( material.mTextures.z, world, n, material.mUVNormalSlope.xy, layerSampler ).r;
        }
    }
    if ( surface.mOptions.z > .5 ) {
        float4 scored = select( weights > 1e-6, weights + detailHeights, -1e4 );
        float maximum = max( max( scored.x, scored.y ), max( scored.z, scored.w ) );
        float4 blended = max( scored - maximum + max( surface.mOptions.y, .001 ), 0 ) * weights;
        weights = blended / max( dot( blended, 1 ), 1e-6 );
    }
    float3 albedo = 0;
    float roughness = 0;
    float3 perturbation = 0;
    for ( uint layer = 0; layer < kTerrainMaterialLayerCount; ++layer ) {
        TerrainLayerData material = surface.mLayers[layer];
        albedo += colors[layer].rgb * weights[layer];
        roughness += material.mTintRoughness.w * weights[layer];
        if ( material.mTextures.y != 0xffffffffu ) {
            Texture2D<float4> source = terrainTextures[NonUniformResourceIndex( material.mTextures.y )];
            float2 scale = material.mUVNormalSlope.xy;
            float2 nx = source.Sample( layerSampler, world.zy * scale ).xy * 2 - 1;
            float2 ny = source.Sample( layerSampler, world.xz * scale ).xy * 2 - 1;
            float2 nz = source.Sample( layerSampler, world.xy * scale ).xy * 2 - 1;
            float3 projections = pow( abs( n ), 4 );
            projections /= max( dot( projections, 1 ), 1e-5 );
            // Rotate each projection's tangent axes into world space before
            // combining detail; flat normal maps retain the geometric slope.
            float3 detail = float3( 0, nx.y, nx.x ) * projections.x +
                            float3( ny.x, 0, ny.y ) * projections.y + float3( nz.x, nz.y, 0 ) * projections.z;
            detail -= n * dot( n, detail );
            perturbation += detail * material.mUVNormalSlope.z * weights[layer];
        }
    }
    n = normalize( n + perturbation );
    roughness = clamp( roughness, .04, 1 );
    // Path tracing presents this legacy terrain overlay after tone mapping;
    // it must not load the raster renderer's absent lighting buffers.
    float3 color = surface.mLighting.w > .5 ? albedo : 0;
    float viewDepth = -mul( camera.mViewMatrix, float4( world, 1 ) ).z;
    for ( uint index = 0; index < uint( surface.mLighting.x ); ++index ) {
        LightInfo light = LoadBufferValue<LightInfo>( surface.mLightBuffer, index * sizeof( LightInfo ) );
        if ( light.mActiveLightType == LIGHT_TYPE_INACTIVE || light.mActiveLightType == LIGHT_TYPE_AREA ) {
            continue;
        }
        float3 delta = light.mPosition.xyz - world;
        float distance = max( length( delta ), .001 );
        float3 l = light.mActiveLightType == LIGHT_TYPE_DIRECTIONAL ? normalize( -light.mDirection.xyz ) : delta / distance;
        float attenuation = light.mActiveLightType == LIGHT_TYPE_DIRECTIONAL ? 1 : pow( saturate( 1 - pow( distance / max( light.mRadius, .001 ), 4 ) ), 2 ) / ( distance * distance );
        if ( light.mActiveLightType == LIGHT_TYPE_SPOT ) {
            attenuation *= smoothstep( light.mOuterCutOff, max( light.mCutOff, light.mOuterCutOff + .0001 ), dot( -l, normalize( light.mDirection.xyz ) ) );
        }
        float3 incoming = light.mDiffuse.rgb * light.mIntensity * attenuation;
        if ( light.mDirection.w > .5 && camera.mAtmosphere != 0 ) {
            AtmosphereData atmosphere = LoadBufferValue<AtmosphereData>( camera.mAtmosphere, 0 );
            incoming = AtmosSurfaceSun( atmosphere, world, terrainTextures[camera.mAtmosphereTransmittance], terrainSamplers[camera.mAtmosphereSampler] );
        }
        if ( light.mActiveLightType == LIGHT_TYPE_DIRECTIONAL ) {
            incoming *= CloudSurfaceShadow( camera.mClouds, world, l );
        }
        float3 h = normalize( v + l );
        float nl = saturate( dot( n, l ) );
        float nv = max( saturate( dot( n, v ) ), .001 );
        float nh = saturate( dot( n, h ) );
        float vh = saturate( dot( v, h ) );
        float a2 = pow( roughness, 4 );
        float denominator = nh * nh * ( a2 - 1 ) + 1;
        float distribution = a2 / max( 3.14159265 * denominator * denominator, 1e-6 );
        float k = pow( roughness + 1, 2 ) / 8;
        float visibility = ( nl / ( nl * ( 1 - k ) + k ) ) * ( nv / ( nv * ( 1 - k ) + k ) );
        float3 fresnel = .04 + .96 * pow( 1 - vh, 5 );
        float3 brdf = albedo * ( 1 - fresnel ) / 3.14159265 + distribution * visibility * fresnel / max( 4 * nl * nv, 1e-5 );
        color += brdf * incoming * nl * TerrainSunShadow( surface, light, world, n, viewDepth, position.xyz );
    }
    if ( surface.mEnvironment.x != 0xffffffffu ) {
        SamplerState environmentSampler = terrainSamplers[surface.mEnvironment.z];
        float3 irradiance = terrainEnvironments[surface.mEnvironment.x].SampleLevel( environmentSampler, n, 0 ).rgb;
        float3 reflected = terrainEnvironments[surface.mEnvironment.y].SampleLevel( environmentSampler, reflect( -v, n ), roughness * surface.mEnvironment.w ).rgb;
        color += surface.mLighting.y * ( albedo * irradiance / 3.14159265 + reflected * .04 );
    }
    if ( surface.mOptions.w > .5 ) {
        TerrainElevationColors palette = parameters.mElevationColors[0];
        color = height < .5 ? lerp( palette.mLow.rgb, palette.mMiddle.rgb, saturate( height * 2 ) ) : lerp( palette.mMiddle.rgb, palette.mHigh.rgb, saturate( height * 2 - 1 ) );
    }
    // Like the water debug view, draw cell edges and vertex dots in pixel units.
    // Both are part of the depth-tested terrain surface; hidden vertices stay hidden.
    const float3 debugColor = float3( 0.0f, 1.0f, 0.0f );
    float2 derivative = max( fwidth( grid ), 1e-5f );
    float2 distance = abs( grid - round( grid ) ) / derivative;
    float visibility = 1.0f - smoothstep( 0.5f, 1.5f, max( derivative.x, derivative.y ) );

    if ( IsDebugView( TerrainDebugFlag::eGridLines ) ) {
        float line = 1.0f - smoothstep( 0.4f, 1.2f, min( distance.x, distance.y ) );
        color = lerp( color, debugColor, line * visibility );
    }

    if ( IsDebugView( TerrainDebugFlag::eVertices ) ) {
        float point = 1.0f - smoothstep( 1.8f, 2.8f, length( distance ) );
        color = lerp( color, debugColor, point * visibility );
    }

    return float4( max( color, 0 ), 1 );
}
