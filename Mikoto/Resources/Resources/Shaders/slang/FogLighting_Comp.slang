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

// Evaluate density and incident illumination in each camera froxel. Geometry and medium
// visibility attenuate the lights before scattering is stored for integration.

#include "base/AreaLighting_Helpers.slang"
#include "base/Clouds_Helpers.slang"
#include "Fog_Common.slang"

// Lighting injection and temporal reprojection reference:
// https://github.com/godotengine/godot/blob/master/servers/rendering/renderer_rd/shaders/environment/volumetric_fog_process.glsl

[numthreads( kVolumeGroupSide, kVolumeGroupSide, kVolumeGroupSide )] void main( uint3 cell : SV_DispatchThreadID ) {
    FogFrameData frame = LoadBufferValue<FogFrameData>( parameters.mFrame, 0 );
    if ( any( cell >= frame.mGrid.xyz ) ) {
        return;
    }
    CameraInfo camera = LoadBufferValue<CameraInfo>( parameters.mCamera, 0 );
    uint random = cell.x * 1973u + cell.y * 9277u + cell.z * 26699u + frame.mFrame.x * 31847u;
    float2 uv = ( float2( cell.xy ) + 0.5 ) / float2( frame.mGrid.xy );
    float3 origin;
    float3 direction;
    FogCameraRay( camera.mInverseViewProjection, camera.mViewPosition.xyz, camera.mProjection[3][3] != 0.0, uv, origin, direction );
    float distance = FogSliceDistance( cell.z + 0.05 + 0.9 * FogRandom( random ), frame );
    float3 position = origin + distance * direction;
    float3 source = 0.0;
    float3 extinction = 0.0;
    float3 ambient = 0.0;
    if ( parameters.mAmbientScale > 0.0 ) {
        // Isotropic ambient approximation from the renderer's convolved environment.
        // Direct lights retain each medium's anisotropic phase function below.
        const float3 directions[kCubemapFaceCount] = { float3( 1, 0, 0 ), float3( -1, 0, 0 ), float3( 0, 1, 0 ),
            float3( 0, -1, 0 ), float3( 0, 0, 1 ), float3( 0, 0, -1 ) };
        for ( uint face = 0; face < kCubemapFaceCount; ++face ) {
            ambient += cubemaps[parameters.mIrradiance].SampleLevel( samplers[parameters.mSampler], directions[face], 0.0 ).rgb;
        }
        // Mikoto's irradiance cubemap is already normalized by pi for diffuse shading.
        ambient *= parameters.mAmbientScale / 6.0;
    }

    // Reuse the surface light clusters. Overflow preserves the complete light list.
    float3 viewPosition = mul( camera.mViewMatrix, float4( position, 1.0 ) ).xyz;
    uint clusterIndex = ComputeTileIndex( viewPosition, float3( parameters.mLightGridXY, parameters.mLightGridZ ), camera.mPlanes.xy,
        float4( uv * camera.mScreenDimensions.xy, 0.0, 1.0 ), camera.mScreenDimensions.xy );
    bool overflow = clusterIndex >= parameters.mClusterCount;
    uint lightCount = parameters.mLightCount;

    if ( !overflow ) {
        ClusterHeader cluster = LoadClusterHeader( parameters.mClusters, clusterIndex );
        overflow = cluster.mPadding[0] != 0;
        lightCount = overflow ? parameters.mLightCount : min( cluster.mCount, MAX_LIGHT_CLUSTERS );
    }

    for ( uint index = 0; index < VolumeCount( parameters.mVolumes ); ++index ) {
        VolumeRecord medium = LoadVolume( parameters.mVolumes, index );
        float density = VolumeDensity( parameters.mVolumes, medium, position );
        extinction += density * ( medium.mAbsorptionDensity.xyz + medium.mScatteringPhase.xyz );
        if ( density <= 0.0 || !any( medium.mScatteringPhase.xyz > 0.0 ) ) {
            continue;
        }
        source += density * medium.mScatteringPhase.xyz * ambient;
        for ( uint lightEntry = 0; lightEntry < lightCount; ++lightEntry ) {
            uint lightIndex = overflow ? lightEntry : LoadClusterLight( parameters.mClusters, clusterIndex, lightEntry );
            if ( lightIndex >= parameters.mLightCount ) {
                continue;
            }

            LightInfo light = LoadBufferValue<LightInfo>( parameters.mLights, lightIndex * sizeof( LightInfo ) );
            if ( light.mActiveLightType == LIGHT_TYPE_INACTIVE ) {
                continue;
            }
            bool directional = light.mActiveLightType == LIGHT_TYPE_DIRECTIONAL;
            float3 toLight = directional ? -light.mDirection.xyz : light.mPosition.xyz - position;
            float lightDistance = directional ? 1e30 : length( toLight );
            if ( lightDistance <= 1e-6 || dot( toLight, toLight ) <= 1e-12 ) {
                continue;
            }
            float3 incoming = normalize( toLight );
            float attenuation = 1.0;
            float3 emission = 1;
            if ( !directional ) {
                float window = saturate( 1.0 - pow( lightDistance / max( light.mRadius, 1e-6 ), 4.0 ) );
                attenuation = window * window / max( lightDistance * lightDistance, 1e-6 );
                if ( light.mActiveLightType == LIGHT_TYPE_SPOT ) {
                    attenuation *= saturate( ( dot( -incoming, normalize( light.mDirection.xyz ) ) - light.mOuterCutOff ) /
                                             max( light.mCutOff - light.mOuterCutOff, 1e-6 ) );
                }
            }
            if ( light.mActiveLightType == LIGHT_TYPE_AREA ) {
                // Dominant-direction approximation for volumetric lighting; surface shading uses the full shape below.
                AreaLightSample area = SampleAreaLight( light, position, incoming, incoming, 1.0 );
                incoming = area.mDiffuseDirection;
                attenuation = area.mIrradiance;
                if ( light.mAreaEmission.x != 0xffffffffu ) {
                    emission = AreaDiffuseTexture( light, position, incoming,
                        textures[NonUniformResourceIndex( light.mAreaEmission.x )], samplers[NonUniformResourceIndex( light.mAreaEmission.y )] );
                }
            }
            if ( attenuation <= 0.0 ) {
                continue;
            }
            float3 visibility = 1.0;
            if ( medium.mFlags.w != 0 ) {
                float geometryVisibility = FogGeometryVisibility( light, position, camera );
                if ( geometryVisibility <= 0.0 ) {
                    continue;
                }

                visibility = geometryVisibility * FogLightTransmittance( position, incoming, lightDistance );
            }
            if ( directional ) {
                visibility *= CloudSurfaceShadow( camera.mClouds, position, incoming );
            }
            source += density * medium.mScatteringPhase.xyz * light.mDiffuse.rgb * emission * light.mIntensity * attenuation * visibility *
                      VolumePhaseHG( dot( -direction, incoming ), medium.mScatteringPhase.w );
        }
    }

    if ( frame.mRange.w > 0.0 ) {
        float4 clip = mul( frame.mPreviousViewProjection, float4( position, 1.0 ) );
        float2 oldUV = float2( clip.x / clip.w * 0.5 + 0.5, 0.5 - clip.y / clip.w * 0.5 );
        if ( clip.w > 0.0 && all( oldUV >= 0.0 ) && all( oldUV <= 1.0 ) ) {
            float3 oldOrigin;
            float3 oldDirection;
            FogCameraRay( frame.mPreviousInverseViewProjection, frame.mPreviousCamera.xyz, frame.mFrame.y != 0, oldUV, oldOrigin, oldDirection );
            float oldDistance = dot( position - oldOrigin, oldDirection );
            if ( oldDistance > 0.0 && oldDistance < frame.mRange.z ) {
                float slice = sqrt( oldDistance / frame.mRange.z ) * frame.mGrid.z - 0.5;
                source = lerp( source, SampleFogVolume( parameters.mHistory, oldUV, slice, frame.mGrid ).rgb, frame.mRange.w );
                extinction = lerp( extinction, SampleFogVolume( parameters.mExtinctionHistory, oldUV, slice, frame.mGrid ).rgb, frame.mRange.w );
            }
        }
    }
    uint2 pixel = FogAtlasPixel( cell, frame.mGrid );
    images[parameters.mLighting][pixel] = float4( min( source, 60000.0 ), 0.0 );
    images[parameters.mExtinction][pixel] = float4( min( extinction, 60000.0 ), 0.0 );
}
