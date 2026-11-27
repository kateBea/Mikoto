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

// Combine water reflection, transmission, absorption, foam and direct lighting. Scene depth
// locates the underwater path; atmosphere/fog attenuate the final surface radiance toward the
// camera.

#include "base/AreaLighting_Helpers.slang"
#include "base/Atmosphere_Helpers.slang"
#include "base/Camera_Helpers.slang"
#include "base/Clouds_Helpers.slang"
#include "base/LightInfo.slang"
#include "base/Tonemap_Helpers.slang"
#include "base/Water_Helpers.slang"

MKT_DECLARE_PUSH_CONSTANT( WaterParameters, parameters );
MKT_DECLARE_TEXTURE_SRV( Texture2D )
textures;
MKT_DECLARE_TEXTURE_SRV( TextureCube )
texturesCube;
MKT_DECLARE_SAMPLERS samplers;

#include "base/MediumLighting_Helpers.slang"
#include "base/WaterMedium_Helpers.slang"

// Copied/adapted from the engine's PBR equations. Water and glass have no metallic
// lobe: F0 comes from the air/material IOR ratio, not hardcoded 4% reflectance.
// Refraction validates four footprint corners; gradient samples use a symmetric subpixel
// offset.
static const uint kWaterRefractionTapCount = 4;
static const float kWaterGradientSampleOffset = .35;

float WaterFresnel( float cosine, float f0 ) {
    return f0 + ( 1 - f0 ) * pow( 1 - saturate( cosine ), 5.0 );
}

// Evaluate the water interface BRDF for one incident light direction. Fresnel splits reflected
// energy from the body contribution while GGX controls the specular highlight.
float3 WaterDirect( float3 n, float3 v, float3 l, float3 radiance,
    float3 tint, float roughness, float f0, float transmission ) {
    float nl = saturate( dot( n, l ) );
    float nv = max( dot( n, v ), 0.001 );
    float3 halfVector = l + v;
    float3 h = halfVector * rsqrt( max( dot( halfVector, halfVector ), 1e-10 ) );
    float nh = saturate( dot( n, h ) );
    float alpha = roughness * roughness;
    float alphaSquared = alpha * alpha;
    float denominator = ( nh * alphaSquared - nh ) * nh + 1;
    float distribution = alphaSquared / max( 3.14159265 * denominator * denominator, 1e-10 );
    float maskingL = 2 * nl / max( nl + sqrt( alphaSquared + ( 1 - alphaSquared ) * nl * nl ), 1e-6 );
    float maskingV = 2 * nv / ( nv + sqrt( alphaSquared + ( 1 - alphaSquared ) * nv * nv ) );
    float fresnel = WaterFresnel( dot( v, h ), f0 );
    float specular = fresnel * maskingL * maskingV * distribution / max( 4 * nl * nv, 1e-6 );
    float3 body = ( 1 - fresnel ) * ( 1 - transmission ) * tint / 3.14159265;
    return radiance * nl * ( body + specular );
}

// Wicked oceanSurfacePS.hlsl: validate full distortion first, attenuate it by
// water depth, and fall back to the undistorted pixel at foreground silhouettes.
// Reconstruct an opaque depth sample and reject it when it lies in front of the water
// interface. Refraction must not pull foreground geometry into the underwater background.
bool WaterReadBottom( Texture2D depth, CameraInfo camera, WaterSurface surface,
    float2 uv, float waterViewDepth, out float3 point, out float gap ) {
    point = 0;
    gap = 1e20;
    if ( any( uv < 0 ) || any( uv > 1 ) ) {
        return false;
    }
    uint width;
    uint height;
    depth.GetDimensions( width, height );
    int2 pixel = clamp( int2( uv * float2( width, height ) ), int2( 0 ), int2( width, height ) - 1 );
    float raw = depth.Load( int3( pixel, 0 ) ).r;
    if ( raw >= 1 ) {
        return false;
    }
    float4 view = mul( camera.mInverseProjection, float4( uv * float2( 2, -2 ) + float2( -1, 1 ), raw, 1 ) );
    if ( abs( view.w ) < 1e-8 ) {
        return false;
    }
    view /= view.w;
    point = camera.mViewPosition.xyz + mul( transpose( ( float3x3 )camera.mViewMatrix ), view.xyz );
    gap = -view.z - waterViewDepth;
    return gap >= -.01 && WaterDepth( surface, point ) >= -.02;
}

// Find a depth-valid refracted background sample and estimate its underwater path length.
// Roughness widens the sample footprint while shoreline validation limits silhouette leaks.
float3 WaterTransmission( WaterVaryings input, CameraInfo camera, WaterSurface surface,
    float3 normal, float roughness, inout float pathLength, out float depthBelow,
    out float3 bottom, out float shoreGap ) {
    depthBelow = surface.mBounds.z;
    bottom = input.mWorldPosition - WaterUp( surface ) * depthBelow;
    shoreGap = 1e20;
    if ( parameters.mSceneColor == 0xffffffffu ) {
        return float3( .12, .17, .22 );
    }
    Texture2D background = textures[parameters.mSceneColor];
    Texture2D depth = textures[parameters.mSceneDepth];
    SamplerState sampler = samplers[parameters.mScreenSampler];
    uint width;
    uint height;
    background.GetDimensions( width, height );
    float2 dimensions = float2( width, height );
    float2 originalUv = input.mPosition.xy / dimensions;
    float3 relative = input.mWorldPosition - camera.mViewPosition.xyz;
    float waterViewDepth = -mul( ( float3x3 )camera.mViewMatrix, relative ).z;
    float3 originalBottom;
    bool originalValid = WaterReadBottom( depth, camera, surface, originalUv, waterViewDepth, originalBottom, shoreGap );
    if ( originalValid ) {
        bottom = originalBottom;
    }

    float3 slope = normal - WaterUp( surface ) * dot( normal, WaterUp( surface ) );
    float2 distortion = mul( ( float3x3 )camera.mViewMatrix, slope ).xy * float2( .1, -.1 );
    float2 uv = originalUv;
    float3 candidate;
    float gap;
    // Check nearby opaque samples before accepting full perturbation. Wicked's
    // reversed-depth max search becomes a foreground validity test in Mikoto.
    bool valid = true;
    const float2 offsets[kWaterRefractionTapCount] = { float2( -1, -1 ), float2( 1, -1 ), float2( -1, 1 ), float2( 1, 1 ) };
    for ( uint tap = 0; tap < kWaterRefractionTapCount; ++tap ) {
        valid = WaterReadBottom( depth, camera, surface, originalUv + distortion + offsets[tap] / dimensions,
                    waterViewDepth, candidate, gap ) &&
                valid;
    }
    if ( valid ) {
        float shallow = 1 - exp( -max( WaterDepth( surface, candidate ), 0.0 ) );
        float2 perturbed = originalUv + distortion * shallow;
        if ( WaterReadBottom( depth, camera, surface, perturbed, waterViewDepth, candidate, gap ) ) {
            uv = perturbed;
            bottom = candidate;
            originalValid = true;
        }
    }
    if ( originalValid ) {
        depthBelow = max( dot( input.mWorldPosition - bottom, WaterUp( surface ) ), 0.0 );
    }
    if ( ( surface.mGeometry.w & kWaterDepthRefraction ) != 0 ) {
        pathLength = originalValid ? min( length( bottom - input.mWorldPosition ), surface.mBounds.z * 5 ) : surface.mBounds.z;
    }

    // Roughness taps use the same foreground rejection as the centre sample.
    float2 blur = roughness * roughness * 4 / dimensions;
    float3 result = 0;
    float total = 0;
    for ( uint tap = 0; tap < kWaterRefractionTapCount; ++tap ) {
        float2 delta = tap < 2 ? float2( ( tap == 0 ? 1 : -1 ) * blur.x, 0 ) : float2( 0, ( tap == 2 ? 1 : -1 ) * blur.y );
        float2 sampleUv = clamp( uv + delta, .5 / dimensions, 1 - .5 / dimensions );
        if ( WaterReadBottom( depth, camera, surface, sampleUv, waterViewDepth, candidate, gap ) || !originalValid ) {
            result += background.SampleLevel( sampler, sampleUv, 0 ).rgb;
            total += 1;
        }
    }
    return total > 0 ? result / total : background.SampleLevel( sampler, uv, 0 ).rgb;
}

// Interpolate adjacent atmosphere depth slices from the 2D aerial-perspective atlas. Radiance
// and transmittance use identical atlas coordinates.
float3 WaterAirVolume( uint textureID, float2 uv, float slice, SamplerState sourceSampler ) {
    float low = floor( slice );
    float high = min( low + 1, kAtmosAerialLastSlice );
    float2 p = clamp( uv * kAtmosAerialSliceSize, .5, ( float( kAtmosAerialSliceSize ) - .5 ) );
    float3 a = textures[textureID].SampleLevel( sourceSampler, float2( p.x / kAtmosAerialSliceSize, ( p.y + low * kAtmosAerialSliceSize ) / kAtmosAerialAtlasHeight ), 0 ).rgb;
    float3 b = textures[textureID].SampleLevel( sourceSampler, float2( p.x / kAtmosAerialSliceSize, ( p.y + high * kAtmosAerialSliceSize ) / kAtmosAerialAtlasHeight ), 0 ).rgb;
    return lerp( a, b, frac( slice ) );
}

// Evaluate atmosphere between the camera and the water surface. Underwater camera segments are
// handled by the water-medium pass instead.
void WaterAerialPerspective( WaterSurface surface, CameraInfo camera, float2 uv, float3 world,
    out float3 radiance, out float3 transmittance ) {
    radiance = 0;
    transmittance = 1;
    if ( surface.mAirResources.x == 0xffffffffu || camera.mAtmosphere == 0 || WaterDepth( surface, camera.mViewPosition.xyz ) > 0 ) {
        return;
    }
    AtmosphereData atmosphere = LoadBufferValue<AtmosphereData>( camera.mAtmosphere, 0 );
    if ( atmosphere.mView.y < .5 ) {
        return;
    }
    float distance = length( world - camera.mViewPosition.xyz ) * atmosphere.mRadii.z;
    float slice = sqrt( saturate( distance / atmosphere.mGround.w ) ) * kAtmosAerialLastSlice;
    SamplerState sourceSampler = samplers[surface.mAirResources.z];
    radiance = WaterAirVolume( surface.mAirResources.x, uv, slice, sourceSampler );
    transmittance = WaterAirVolume( surface.mAirResources.y, uv, slice, sourceSampler );
    if ( slice < 1 ) {
        radiance *= slice;
        transmittance = lerp( float3( 1 ), transmittance, slice );
    }
}

[shader( "fragment" )]
float4 main( WaterVaryings input ) : SV_Target0 {
    BufferReference<WaterSurface> surfaces = ( BufferReference<WaterSurface> )parameters.mSurfaces;
    WaterSurface surface = surfaces[parameters.mSurface];
    BufferReference<CameraInfo> cameras = ( BufferReference<CameraInfo> )parameters.mCamera;
    CameraInfo camera = cameras[0];
    float3 v = normalize( camera.mViewPosition.xyz - input.mWorldPosition );
    // Per-pixel tangents follow the displaced surface, independent of mesh density.
    float3 tangentX;
    float3 tangentZ;
    if ( surface.mSimulation.z == kWaterSimulationFft ) {
        WaterFftSample sample = GetWaterFftSample( input.mRestPosition, surface.mFft.x );
        uint firstPair = surface.mSimulation.w * 2;
        tangentX = 0;
        tangentZ = 0;
        float2 footprint = min( fwidth( input.mRestPosition ), float2( .25 / surface.mFft.x ) );
        for ( uint tap = 0; tap < kBilinearTapCount; ++tap ) {
            float2 offset = float2( ( tap & 1 ) != 0 ? kWaterGradientSampleOffset : -kWaterGradientSampleOffset, ( tap & 2 ) != 0 ? kWaterGradientSampleOffset : -kWaterGradientSampleOffset ) * footprint;
            WaterFftSample filtered = GetWaterFftSample( input.mRestPosition + offset, surface.mFft.x );
            tangentX += SampleWaterFft( parameters.mFftGradients, firstPair, filtered, 2 ).xyz * .25;
            tangentZ += SampleWaterFft( parameters.mFftGradients, firstPair + 1, filtered, 2 ).xyz * .25;
        }
    }
    else {
        float3 position;
        EvaluateWater( float3( input.mRestPosition.x, 0, input.mRestPosition.y ),
            surface, parameters.mWaves, position, tangentX, tangentZ );
    }
    // Suppress water over exposed terrain and attenuate waves toward shallow water. Coast-field
    // gradients also modify the displaced tangents near shore.
    float bedDepth = WaterBedDepth( surface, input.mRestPosition );
    clip( bedDepth + .05 );
    float weight = WaterWaveWeight( surface, input.mRestPosition );
    float2 step = surface.mCoastPatch.zw * .25;
    float2 gradient = float2(
                          WaterWaveWeight( surface, input.mRestPosition + float2( step.x, 0 ) ) - WaterWaveWeight( surface, input.mRestPosition - float2( step.x, 0 ) ),
                          WaterWaveWeight( surface, input.mRestPosition + float2( 0, step.y ) ) - WaterWaveWeight( surface, input.mRestPosition - float2( 0, step.y ) ) ) /
                      ( 2 * step );
    float3 displacement;
    if ( surface.mSimulation.z == kWaterSimulationFft ) {
        displacement = SampleWaterFft( parameters.mFftDisplacement, surface.mSimulation.w,
            GetWaterFftSample( input.mRestPosition, surface.mFft.x ), 1 )
                           .xyz;
    }
    else {
        float3 unusedX;
        float3 unusedZ;
        float3 rest = float3( input.mRestPosition.x, 0, input.mRestPosition.y );
        EvaluateWater( rest, surface, parameters.mWaves, displacement, unusedX, unusedZ );
        displacement -= rest;
    }
    tangentX = float3( 1, 0, 0 ) + ( tangentX - float3( 1, 0, 0 ) ) * weight + displacement * gradient.x;
    tangentZ = float3( 0, 0, 1 ) + ( tangentZ - float3( 0, 0, 1 ) ) * weight + displacement * gradient.y;
    float3 worldX = mul( ( float3x3 )surface.mWorld, tangentX );
    float3 worldZ = mul( ( float3x3 )surface.mWorld, tangentZ );
    float3 n = cross( worldZ, worldX );
    n *= rsqrt( max( dot( n, n ), 1e-20 ) );
    bool underside = dot( n, v ) < 0;
    if ( underside ) {
        n = -n;
    }

    // Estimate how much wave detail the current pixel can resolve. Unresolved slopes fade
    // toward the mean normal while roughness retains their lost specular variance.
    float footprint = max( length( ddx( input.mRestPosition ) ), length( ddy( input.mRestPosition ) ) );
    float unresolved = smoothstep( .5, 6.0, footprint );
    float3 up = underside ? -WaterUp( surface ) : WaterUp( surface );
    n = normalize( lerp( n, up, unresolved ) );

    // The determinant measures horizontal area compression: 1 is flat water,
    // smaller values mark compressed crests. This is instant whitecap coverage,
    // not a persistent foam simulation. Sine-only waves have determinant 1.
    float jacobian = tangentX.x * tangentZ.z - tangentX.z * tangentZ.x;
    float crest = pow( saturate( 1 - jacobian ), 4 );
    float foam = 0;
    // Compute interface Fresnel and the refracted direction from the water IOR. The underside
    // flips the index ratio and may produce total internal reflection.
    float ior = surface.mOptics.x;
    float transmission = surface.mOptics.y;
    float thickness = surface.mOptics.z;
    // Raise the specular lobe width as normal frequencies become unresolved.
    float roughness = sqrt( lerp( surface.mColorRoughness.a * surface.mColorRoughness.a, max( surface.mColorRoughness.a * surface.mColorRoughness.a, .35 * .35 ), unresolved ) );
    float3 tint = surface.mColorRoughness.rgb;
    float ratio = ( ior - 1 ) / ( ior + 1 );
    float f0 = ratio * ratio;
    float nv = max( dot( n, v ), 0.001 );
    float fresnel = WaterFresnel( nv, f0 );
    float3 refracted = refract( -v, n, underside ? ior : 1 / ior );
    bool totalInternalReflection = dot( refracted, refracted ) < 1e-8;
    if ( totalInternalReflection ) {
        fresnel = 1;
    }
    float pathLength = thickness / max( abs( dot( refracted, n ) ), 0.1 );
    float depthBelow;
    float3 bottom;
    float shoreGap;
    float3 behind = WaterTransmission( input, camera, surface, n, roughness, pathLength, depthBelow, bottom, shoreGap );
    if ( underside && ( surface.mGeometry.w & kWaterUnderwaterScattering ) != 0 ) {
        pathLength = 0;
    }// The post-water pass integrates the camera-to-interface segment.
    // Apply Beer-Lambert attenuation along the underwater path. Absorption is wavelength-
    // dependent, so the background tint changes with optical distance.
    float3 extinction = WaterExtinction( surface );
    float3 absorption = exp( -extinction * pathLength );
    if ( !underside ) {
        float coverage = crest * surface.mOptics.w * exp( -max( depthBelow, 0 ) * .1 );
        if ( ( surface.mGeometry.w & kWaterShoreFoam ) != 0 ) {
            float shore = exp( -max( shoreGap, 0 ) * 2 / max( surface.mCoast.x, .01 ) ) * surface.mCoast.y;
            float bathymetry = saturate( 1 - max( bedDepth, 0 ) / max( surface.mCoast.x, .01 ) ) * surface.mCoast.y;
            coverage += max( shore, WaterPersistentFoam( surface, input.mRestPosition, bathymetry ) );
        }
        foam = WickedWaterFoam( saturate( coverage ), input.mRestPosition, surface.mBounds.w, footprint );
    }
    float3 mediumRadiance = 0;
    float3 causticLight = 0;

    float3 color = float3( 0 );
    float3 foamLighting = float3( 0 );
    // Accumulate direct reflection and medium illumination from the scene lights. Geometry
    // visibility and water transmittance attenuate the incident light before it contributes.
    BufferReference<LightInfo> lights = ( BufferReference<LightInfo> )parameters.mLights;
    for ( uint i = 0; i < parameters.mLightCount; ++i ) {
        LightInfo light = lights[i];
        if ( light.mActiveLightType == LIGHT_TYPE_AREA ) {
            AreaSurfaceLighting area = EvaluateAreaLight( light, input.mWorldPosition, n, v,
                tint * ( 1.0 - transmission ), float3( f0 ), float3( 1.0 ), roughness );
            float3 diffuseEmission = 1;
            if ( light.mAreaEmission.x != 0xffffffffu ) {
                Texture2D source = textures[NonUniformResourceIndex( light.mAreaEmission.x )];
                SamplerState sourceSampler = samplers[NonUniformResourceIndex( light.mAreaEmission.y )];
                diffuseEmission = AreaDiffuseTexture( light, input.mWorldPosition, n, source, sourceSampler );
                area.mDiffuse *= diffuseEmission;
                area.mSpecular *= AreaSpecularTexture( light, area, roughness, source, sourceSampler );
            }
            float3 radiance = light.mDiffuse.xyz * light.mIntensity;
            color += radiance * ( area.mDiffuse + area.mSpecular );
            foamLighting += radiance * diffuseEmission * SampleAreaLight( light, input.mWorldPosition, n, v, roughness ).mIrradiance / PI_Value;
            continue;
        }
        if ( light.mActiveLightType < 0 || light.mActiveLightType > LIGHT_TYPE_SPOT ) {
            continue;
        }
        float3 l;
        float attenuation = 1;
        if ( light.mActiveLightType == LIGHT_TYPE_DIRECTIONAL ) {
            l = normalize( -light.mDirection.xyz );
        }
        else {
            float3 delta = light.mPosition.xyz - input.mWorldPosition;
            float distanceSquared = max( dot( delta, delta ), 1e-6 );
            l = delta * rsqrt( distanceSquared );
            float radius = max( light.mRadius, 0.001 );
            if ( light.mActiveLightType == LIGHT_TYPE_POINT ) {
                float falloff = saturate( 1 - pow( distanceSquared / ( radius * radius ), 2 ) );
                attenuation = falloff * falloff / distanceSquared;
            }
            else {
                float falloff = saturate( 1 - sqrt( distanceSquared ) / radius );
                float cosine = dot( normalize( -light.mDirection.xyz ), l );
                float cone = light.mCutOff - light.mOuterCutOff > 1e-6 ? saturate( ( cosine - light.mOuterCutOff ) / ( light.mCutOff - light.mOuterCutOff ) ) : ( cosine >= light.mOuterCutOff ? 1.0 : 0.0 );
                attenuation = 1000 * falloff * falloff * cone;
            }
        }
        float3 visibility = MediumGeometryVisibility( light, input.mWorldPosition, camera, parameters.mShadows, parameters.mShadowCount );
        if ( light.mActiveLightType == LIGHT_TYPE_DIRECTIONAL ) {
            visibility *= CloudSurfaceShadow( camera.mClouds, input.mWorldPosition, l );
            float3 radiance = light.mDiffuse.rgb * light.mIntensity * visibility;
            float3 entryTransmission = WaterSunTransmittance( surface, input.mWorldPosition - WaterUp( surface ) * .5 * depthBelow, l );
            if ( !underside && ( surface.mGeometry.w & kWaterUnderwaterScattering ) != 0 ) {
                mediumRadiance += surface.mScattering.rgb * radiance * entryTransmission *
                                  VolumePhaseHG( dot( v, WaterLightDirection( surface, l ) ), surface.mScattering.w ) * WaterSegmentIntegral( extinction, pathLength );
            }
            float bottomVisibility = MediumGeometryVisibility( light, bottom, camera, parameters.mShadows, parameters.mShadowCount );
            causticLight += radiance * bottomVisibility * WaterSunTransmittance( surface, bottom, l ) * WaterCausticPattern( surface, bottom, l );
        }
        light.mDiffuse.rgb *= visibility;
        color += WaterDirect( n, v, l, light.mDiffuse.rgb * light.mIntensity * attenuation,
            tint, roughness, f0, transmission );
        foamLighting += light.mDiffuse.rgb * light.mIntensity * attenuation * saturate( dot( n, l ) ) / 3.14159265;
    }

    if ( parameters.mEnvironment != 0xffffffffu ) {
        TextureCube environment = texturesCube[parameters.mEnvironment];
        SamplerState sampler = samplers[parameters.mSampler];
        uint width;
        uint height;
        uint levels;
        environment.GetDimensions( 0, width, height, levels );
        float3 reflection = environment.SampleLevel( sampler, reflect( -v, n ), roughness * ( levels - 1 ) ).rgb;
        float2 brdf = textures[parameters.mBrdf].SampleLevel( sampler, float2( nv, 1 - roughness ), 0 ).rg;
        float3 irradiance = texturesCube[parameters.mIrradiance].SampleLevel( sampler, n, 0 ).rgb;
        foamLighting += parameters.mEnvironmentScale * irradiance;
        float reflectionWeight = totalInternalReflection ? 1 : f0 * brdf.x + brdf.y;
        color += parameters.mEnvironmentScale * ( reflection * reflectionWeight +
                                                   irradiance * tint * ( 1 - transmission ) * ( 1 - fresnel ) );
    }
    else {
        color += 0.15 * tint * ( 1 - transmission ) * ( 1 - fresnel );
        foamLighting += 0.15;
    }
    // Combine the refracted background and in-water light separately from interface reflection.
    // Total internal reflection removes the transmitted branch.
    float3 transmitted = 0;
    if ( !totalInternalReflection ) {
        if ( !underside ) {
            behind *= 1 + min( causticLight, 2.0 );
        }
        // The absorption/scattering coefficients define the transmitted colour.
        float3 transmissionTint = ( surface.mGeometry.w & kWaterDepthRefraction ) != 0 ? float3( 1 ) : tint;
        transmitted = behind * absorption * transmissionTint * transmission * ( 1 - fresnel );
        color += mediumRadiance * transmissionTint * transmission * ( 1 - fresnel );
    }
    // Foam replaces covered water with a lit, nearly white diffuse surface;
    // it does not emit light and it reduces transmission under the whitecap.
    color = lerp( color, float3( .92, .96, 1 ) * foamLighting, foam );
    transmitted *= 1 - foam;
    float3 airRadiance;
    float3 airTransmittance;
    WaterAerialPerspective( surface, camera, input.mPosition.xy / camera.mScreenDimensions.xy, input.mWorldPosition, airRadiance, airTransmittance );
    float3 opacity = totalInternalReflection ? float3( 1 ) : 1 - absorption * transmission * ( 1 - fresnel ) * ( 1 - foam );
    color = color * airTransmittance + airRadiance * opacity + transmitted;

    // Rest-grid coordinates follow lateral motion too; fade subpixel cells.
    float2 derivative = max( fwidth( input.mGrid ), 1e-5 );
    float2 distance = abs( input.mGrid - round( input.mGrid ) ) / derivative;
    float visibility = 1 - smoothstep( 0.5, 1.5, max( derivative.x, derivative.y ) );
    if ( ( surface.mGeometry.w & 1 ) != 0 ) {
        float line = 1 - smoothstep( 0.4, 1.2, min( distance.x, distance.y ) );
        color = lerp( color, surface.mDebugPreviewColor.rgb, line * visibility );
    }
    if ( ( surface.mGeometry.w & 2 ) != 0 ) {
        float point = 1 - smoothstep( 1.8, 2.8, length( distance ) );
        color = lerp( color, surface.mDebugPreviewColor.rgb, point * visibility );
    }
    if ( ( surface.mGeometry.w & 64u ) != 0 && parameters.mSceneColor != 0xffffffffu ) {
        float viewDepth = -mul( ( float3x3 )camera.mViewMatrix, input.mWorldPosition - camera.mViewPosition.xyz ).z;
        float fade = smoothstep( .75, .98, viewDepth / camera.mPlanes.y );
        float3 horizon = textures[parameters.mSceneColor].SampleLevel( samplers[parameters.mScreenSampler],
                                                            input.mPosition.xy / camera.mScreenDimensions.xy, 0 )
                             .rgb;
        color = lerp( color, horizon, fade );
    }
    // Raster water enters HDR before post effects; the path-tracing overlay
    // still targets the display image and needs its own tone mapping.
    return parameters.mToneMap < 0 ? float4( color, 1 ) : ApplyToneMapping( float4( color, 1 ), parameters.mToneMap, camera.mExposure, camera.mGamma );
}
