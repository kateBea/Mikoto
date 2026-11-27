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

// Shade the blade or imported tuft using material coverage, grass color variation and
// directional lighting. Canopy-map visibility and nearby contact shadows complement analytical
// blade self-shadowing.

#include "base/Atmosphere_Helpers.slang"
#include "base/Clouds_Helpers.slang"
#include "base/Grass_Helpers.slang"
#include "base/TerrainLighting_Helpers.slang"

[shader( "fragment" )]
float4 main( float3 world : TEXCOORD0, float3 normal : TEXCOORD1, float3 tangent : TEXCOORD2, float4 shape : TEXCOORD3, bool front : SV_IsFrontFace, float4 position : SV_Position ) : SV_Target0 {
    GrassDescription source = LoadBufferValue<GrassDescription>( parameters.mDescriptions, parameters.mTerrain * sizeof( GrassDescription ) );
    TerrainSurfaceData surface = LoadBufferValue<TerrainSurfaceData>( source.mSurface, 0 );
    CameraInfo camera = LoadBufferValue<CameraInfo>( parameters.mCamera, 0 );
    float height = saturate( shape.x );
    float3 albedo = GrassFlowColor( height, shape.z, source.mRootColor.rgb, source.mMidColor.rgb, source.mTipColor.rgb,
        source.mDryColor.rgb, source.mMidColor.a, source.mDryColor.a );
    if ( source.mMesh.y != 0 ) {
        float4 textureColor = float4( 1, 1, 1, 1 );
        if ( source.mMesh.z != 0xffffffffu ) {
            textureColor = terrainTextures[source.mMesh.z].Sample( terrainSamplers[surface.mControls.x], shape.yw );
            if ( source.mMesh.w != 0 ) {
                float3 encoded = max( textureColor.rgb, 0 );
                textureColor.rgb = encoded <= .04045 ? encoded / 12.92 : pow( ( encoded + .055 ) / 1.055, 2.4 );
            }
        }
        if ( source.mMaterialOptions.y != 0 ) {
            clip( textureColor.a * source.mMaterialColor.a - source.mMaterialOptions.x );
        }
        // Textured plants retain their authored petal/leaf colours. Untextured
        // tufts keep the procedural root-to-tip grass colour gradient.
        albedo = lerp( albedo, float3( 1 ), source.mMaterialOptions.z ) * textureColor.rgb * source.mMaterialColor.rgb;
    }
    // Round the normal across the width without modifying the mesh silhouette.
    float angle = ( shape.y * 2 - 1 ) * 3.14159265 * source.mLighting.z * ( 1 - source.mMaterialOptions.z );
    float3 n = normalize( normal * cos( angle ) + cross( tangent, normal ) * sin( angle ) ) * ( front ? 1 : -1 );
    float3 view = GrassFlowNormalize( camera.mViewPosition.xyz - world, n );
    float depth = -mul( camera.mViewMatrix, float4( world, 1 ) ).z;
    float3 color = 0;
    float canopyDepth = ( 1 - height ) * ( 1 - height );
    float selfShadow = saturate( asfloat( source.mControl.y ) );
    float roughness = lerp( clamp( source.mTipColor.a, .25, 1 ), 1,
        smoothstep( source.mView.y, source.mView.x, length( camera.mViewPosition.xyz - world ) ) );
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
        float diffuse = saturate( dot( n, l ) * .5 + .5 );
        float visibility = exp( -selfShadow * canopyDepth * .6 / max( abs( l.y ), .2 ) );
        float3 backLight = -l + n * .3;
        float transmission = pow( saturate( dot( view, backLight ) ), source.mLighting.y ) * source.mLighting.x * height;
        float3 halfVector = GrassFlowNormalize( l + view, n );
        float exponent = exp2( lerp( 7, 2, roughness ) );
        float sheen = .035 * ( exponent + 2 ) / ( 2 * 3.14159265 ) * pow( saturate( dot( n, halfVector ) ), exponent ) * height;
        color += incoming * ( albedo * ( diffuse + transmission ) / 3.14159265 + sheen ) * visibility *
            TerrainSunShadow( surface, light, world, n, depth, position.xyz, true );
    }
    if ( surface.mEnvironment.x != 0xffffffffu ) {
        float3 skyNormal = normalize( float3( n.x, abs( n.y ) + .35, n.z ) );
        float3 irradiance = terrainEnvironments[surface.mEnvironment.x].SampleLevel( terrainSamplers[surface.mEnvironment.z], skyNormal, 0 ).rgb;
        float ao = lerp( 1, saturate( height * 2 + .3 ), saturate( source.mRootColor.a ) );
        color += albedo * irradiance * surface.mLighting.y * ao / 3.14159265;
    }
    return float4( max( color, 0 ), 1 );
}
