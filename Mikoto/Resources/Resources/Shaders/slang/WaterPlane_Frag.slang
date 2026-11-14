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

#include "base/Camera_Helpers.slang"
#include "base/LightInfo.slang"
#include "base/Water_Helpers.slang"
#include "base/Tonemap_Helpers.slang"

MKT_DECLARE_PUSH_CONSTANT( WaterParameters, parameters );
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures2D;
MKT_DECLARE_TEXTURE_SRV( TextureCube ) texturesCube;
MKT_DECLARE_SAMPLERS samplers;

// Copied/adapted from the engine's PBR equations. Water and glass have no metallic
// lobe: F0 comes from the air/material IOR ratio, not hardcoded 4% reflectance.
float WaterFresnel( float cosine, float f0 ) {
    return f0 + (1 - f0) * pow( 1 - saturate( cosine ), 5.0 );
}

// Continuous, derivative-filtered variation breaks up whitecaps without a
// texture or frame-dependent random flicker. Coordinates follow the rest mesh.
float WaterFoamPattern( float2 position ) {
    float2 phase = float2( dot( position, float2( 2.3, 1.7 ) ),
                          dot( position, float2( -3.1, 2.9 ) ) );
    float2 visibility = 1 - smoothstep( 0.5, 3.0, fwidth( phase ) );
    float variation = dot( sin( phase ) * visibility, float2( 0.15, 0.1 ) );
    return saturate( 0.75 + variation );
}

float3 WaterDirect( float3 n, float3 v, float3 l, float3 radiance,
                    float3 tint, float roughness, float f0, float transmission ) {
    float nl = saturate( dot( n, l ) );
    float nv = max( dot( n, v ), 0.001 );
    float3 halfVector = l + v;
    float3 h = halfVector * rsqrt( max( dot( halfVector, halfVector ), 1e-10 ) );
    float nh = saturate( dot( n, h ) );
    float alpha = roughness * roughness;
    float alphaSquared = alpha * alpha;
    float denominator = (nh * alphaSquared - nh) * nh + 1;
    float distribution = alphaSquared / max( 3.14159265 * denominator * denominator, 1e-10 );
    float maskingL = 2 * nl / max( nl + sqrt( alphaSquared + (1 - alphaSquared) * nl * nl ), 1e-6 );
    float maskingV = 2 * nv / (nv + sqrt( alphaSquared + (1 - alphaSquared) * nv * nv ));
    float fresnel = WaterFresnel( dot( v, h ), f0 );
    float specular = fresnel * maskingL * maskingV * distribution / max( 4 * nl * nv, 1e-6 );
    float3 body = (1 - fresnel) * (1 - transmission) * tint / 3.14159265;
    return radiance * nl * (body + specular);
}

// A snapshot avoids reading the active render target. Thickness is an authored
// optical path approximation; this is screen refraction, not ray-traced glass.
float3 WaterTransmission( WaterVaryings input, CameraInfo camera, float3 ray, float thickness, float roughness ) {
    if ( parameters.sceneColor == 0xffffffffu ) { return float3( 0.12, 0.17, 0.22 ); }
    Texture2D background = textures2D[parameters.sceneColor];
    Texture2D depth = textures2D[parameters.sceneDepth];
    SamplerState sampler = samplers[parameters.screenSampler];
    uint width, height;
    background.GetDimensions( width, height );
    float2 uv = input.position.xy / float2( width, height );
    float4 clip = CameraClipPosition( float4( input.worldPosition + ray * thickness, 1 ), camera );
    float2 refractedUv = clip.xy / max( clip.w, 1e-5 ) * float2( 0.5, -0.5 ) + 0.5;
    bool valid = clip.w > 0 && all( refractedUv >= 0 ) && all( refractedUv <= 1 );
    int2 pixel = clamp( int2( refractedUv * float2( width, height ) ), int2( 0 ), int2( width - 1, height - 1 ) );
    // Foreground geometry must not leak into water across its silhouette.
    valid = valid && depth.Load( int3( pixel, 0 ) ).r >= input.position.z;
    uv = valid ? refractedUv : uv;
    float2 blur = roughness * roughness * 4 / float2( width, height );
    return 0.25 * (background.SampleLevel( sampler, uv + float2( blur.x, 0 ), 0 ).rgb +
                   background.SampleLevel( sampler, uv - float2( blur.x, 0 ), 0 ).rgb +
                   background.SampleLevel( sampler, uv + float2( 0, blur.y ), 0 ).rgb +
                   background.SampleLevel( sampler, uv - float2( 0, blur.y ), 0 ).rgb);
}

[shader("fragment")]
float4 main( WaterVaryings input ) : SV_Target0 {
    BufferReference<WaterSurface> surfaces = (BufferReference<WaterSurface>)parameters.surfaces;
    WaterSurface surface = surfaces[parameters.surface];
    BufferReference<CameraInfo> cameras = (BufferReference<CameraInfo>)parameters.camera;
    CameraInfo camera = cameras[0];
    float3 v = normalize( camera.mViewPosition.xyz - input.worldPosition );
    // Per-pixel tangents follow the displaced surface, independent of mesh density.
    float3 tangentX, tangentZ;
    if ( surface.simulation.z == kWaterSimulationFft ) {
        WaterFftSample sample = GetWaterFftSample( input.restPosition, surface.fft.x );
        uint firstPair = surface.simulation.w * 2;
        tangentX = SampleWaterFft( parameters.fftGradients, firstPair, sample, 2 ).xyz;
        tangentZ = SampleWaterFft( parameters.fftGradients, firstPair + 1, sample, 2 ).xyz;
    }
    else {
        float3 position;
        EvaluateWater( float3( input.restPosition.x, 0, input.restPosition.y ),
            surface, parameters.waves, position, tangentX, tangentZ );
    }
    float3 worldX = mul( (float3x3)surface.world, tangentX );
    float3 worldZ = mul( (float3x3)surface.world, tangentZ );
    float3 n = cross( worldZ, worldX );
    n *= rsqrt( max( dot( n, n ), 1e-20 ) );
    bool underside = dot( n, v ) < 0;
    if ( underside ) { n = -n; }

    // The determinant measures horizontal area compression: 1 is flat water,
    // smaller values mark compressed crests. This is instant whitecap coverage,
    // not a persistent foam simulation. Sine-only waves have determinant 1.
    float jacobian = tangentX.x * tangentZ.z - tangentX.z * tangentZ.x;
    float foam = underside ? 0 : surface.optics.w *
        (1 - smoothstep( 0.55, 0.88, jacobian )) * WaterFoamPattern( input.restPosition );
    float ior = surface.optics.x;
    float transmission = surface.optics.y;
    float thickness = surface.optics.z;
    float roughness = surface.colorRoughness.a;
    float3 tint = surface.colorRoughness.rgb;
    float ratio = (ior - 1) / (ior + 1);
    float f0 = ratio * ratio;
    float nv = max( dot( n, v ), 0.001 );
    float fresnel = WaterFresnel( nv, f0 );
    float3 refracted = refract( -v, n, underside ? ior : 1 / ior );
    bool totalInternalReflection = dot( refracted, refracted ) < 1e-8;
    if ( totalInternalReflection ) { fresnel = 1; }
    float pathLength = thickness / max( abs( dot( refracted, n ) ), 0.1 );
    float3 absorption = pow( max( surface.attenuation.rgb, 0.0001 ), pathLength / surface.attenuation.a );

    float3 color = float3( 0 );
    float3 foamLighting = float3( 0 );
    BufferReference<LightInfo> lights = (BufferReference<LightInfo>)parameters.lights;
    for ( uint i = 0; i < parameters.lightCount; ++i ) {
        LightInfo light = lights[i];
        if ( light.ActiveLightType < 0 || light.ActiveLightType > LIGHT_TYPE_SPOT ) { continue; }
        float3 l;
        float attenuation = 1;
        if ( light.ActiveLightType == LIGHT_TYPE_DIRECTIONAL ) {
            l = normalize( -light.Direction.xyz );
        } else {
            float3 delta = light.Position.xyz - input.worldPosition;
            float distanceSquared = max( dot( delta, delta ), 1e-6 );
            l = delta * rsqrt( distanceSquared );
            float radius = max( light.Radius, 0.001 );
            if ( light.ActiveLightType == LIGHT_TYPE_POINT ) {
                float falloff = saturate( 1 - pow( distanceSquared / (radius * radius), 2 ) );
                attenuation = falloff * falloff / distanceSquared;
            } else {
                float falloff = saturate( 1 - sqrt( distanceSquared ) / radius );
                float cosine = dot( normalize( -light.Direction.xyz ), l );
                float cone = light.CutOff - light.OuterCutOff > 1e-6 ?
                    saturate( (cosine - light.OuterCutOff) / (light.CutOff - light.OuterCutOff) ) :
                    (cosine >= light.OuterCutOff ? 1.0 : 0.0);
                attenuation = 1000 * falloff * falloff * cone;
            }
        }
        color += WaterDirect( n, v, l, light.Diffuse.rgb * light.Intensity * attenuation,
                              tint, roughness, f0, transmission );
        foamLighting += light.Diffuse.rgb * light.Intensity * attenuation * saturate( dot( n, l ) ) / 3.14159265;
    }

    if ( parameters.environment != 0xffffffffu ) {
        TextureCube environment = texturesCube[parameters.environment];
        SamplerState sampler = samplers[parameters.sampler];
        uint width, height, levels;
        environment.GetDimensions( 0, width, height, levels );
        float3 reflection = environment.SampleLevel( sampler, reflect( -v, n ), roughness * (levels - 1) ).rgb;
        float2 brdf = textures2D[parameters.brdf].SampleLevel( sampler, float2( nv, 1 - roughness ), 0 ).rg;
        float3 irradiance = texturesCube[parameters.irradiance].SampleLevel( sampler, n, 0 ).rgb;
        foamLighting += parameters.environmentScale * irradiance;
        float reflectionWeight = totalInternalReflection ? 1 : f0 * brdf.x + brdf.y;
        color += parameters.environmentScale * (reflection * reflectionWeight +
            irradiance * tint * (1 - transmission) * (1 - fresnel));
    } else {
        color += 0.15 * tint * (1 - transmission) * (1 - fresnel);
        foamLighting += 0.15;
    }
    if ( !totalInternalReflection ) {
        float3 behind = WaterTransmission( input, camera, refracted, thickness, roughness );
        color += behind * absorption * tint * transmission * (1 - fresnel);
    }
    // Foam replaces covered water with a lit, nearly white diffuse surface;
    // it does not emit light and it reduces transmission under the whitecap.
    color = lerp( color, float3( 0.92, 0.96, 1.0 ) * foamLighting, foam );

    // Rest-grid coordinates follow lateral motion too; fade subpixel cells.
    float2 derivative = max( fwidth( input.grid ), 1e-5 );
    float2 distance = abs( input.grid - round( input.grid ) ) / derivative;
    float visibility = 1 - smoothstep( 0.5, 1.5, max( derivative.x, derivative.y ) );
    if ( (surface.geometry.w & 1) != 0 ) {
        float line = 1 - smoothstep( 0.4, 1.2, min( distance.x, distance.y ) );
        color = lerp( color, surface.debugPreviewColor.rgb, line * visibility );
    }
    if ( (surface.geometry.w & 2) != 0 ) {
        float point = 1 - smoothstep( 1.8, 2.8, length( distance ) );
        color = lerp( color, surface.debugPreviewColor.rgb, point * visibility );
    }
    // Raster water enters HDR before post effects; the path-tracing overlay
    // still targets the display image and needs its own tone mapping.
    return parameters.toneMap < 0 ? float4( color, 1 ) :
        ApplyToneMapping( float4( color, 1 ), parameters.toneMap, camera.mExposure, camera.mGamma );
}
