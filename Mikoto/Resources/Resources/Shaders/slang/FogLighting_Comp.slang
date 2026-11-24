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

#include "Fog_Common.slang"
#include "base/AreaLighting_Helpers.slang"

// Lighting injection and temporal reprojection reference:
// https://github.com/godotengine/godot/blob/master/servers/rendering/renderer_rd/shaders/environment/volumetric_fog_process.glsl

[numthreads( 4, 4, 4 )] void main( uint3 cell : SV_DispatchThreadID ) {
    FogFrameData frame = LoadBufferValue<FogFrameData>( parameters.Frame, 0 );
    if ( any( cell >= frame.Grid.xyz ) ) {
        return;
    }
    CameraInfo camera = LoadBufferValue<CameraInfo>( parameters.Camera, 0 );
    uint random = cell.x * 1973u + cell.y * 9277u + cell.z * 26699u + frame.Frame.x * 31847u;
    float2 uv = ( float2( cell.xy ) + 0.5 ) / float2( frame.Grid.xy );
    float3 origin, direction;
    FogCameraRay( camera.mInverseViewProjection, camera.mViewPosition.xyz, camera.mProjection[3][3] != 0.0, uv, origin, direction );
    float distance = FogSliceDistance( cell.z + 0.05 + 0.9 * FogRandom( random ), frame );
    float3 position = origin + distance * direction;
    float3 source = 0.0, extinction = 0.0;
    float3 ambient = 0.0;
    if ( parameters.AmbientScale > 0.0 ) {
        // Isotropic ambient approximation from the renderer's convolved environment.
        // Direct lights retain each medium's anisotropic phase function below.
        const float3 directions[6] = { float3( 1, 0, 0 ), float3( -1, 0, 0 ), float3( 0, 1, 0 ),
                                       float3( 0, -1, 0 ), float3( 0, 0, 1 ), float3( 0, 0, -1 ) };
        for ( uint face = 0; face < 6; ++face ) {
            ambient += cubemaps[parameters.Irradiance].SampleLevel( samplers[parameters.Sampler], directions[face], 0.0 ).rgb;
        }
        // Mikoto's irradiance cubemap is already normalized by pi for diffuse shading.
        ambient *= parameters.AmbientScale / 6.0;
    }

    // Reuse the surface light clusters. Overflow preserves the complete light list.
    float3 viewPosition = mul( camera.mViewMatrix, float4( position, 1.0 ) ).xyz;
    uint clusterIndex = ComputeTileIndex( viewPosition, float3( parameters.LightGridXY, parameters.LightGridZ ), camera.mPlanes.xy,
                                          float4( uv * camera.mScreenDimensions.xy, 0.0, 1.0 ), camera.mScreenDimensions.xy );
    bool overflow = clusterIndex >= parameters.ClusterCount;
    uint lightCount = parameters.LightCount;

    if ( !overflow ) {
        ClusterHeader cluster = LoadClusterHeader( parameters.Clusters, clusterIndex );
        overflow = cluster.mPadding[0] != 0;
        lightCount = overflow ? parameters.LightCount : min( cluster.mCount, MAX_LIGHT_CLUSTERS );
    }

    for ( uint index = 0; index < VolumeCount( parameters.Volumes ); ++index ) {
        VolumeRecord medium = LoadVolume( parameters.Volumes, index );
        float density = VolumeDensity( parameters.Volumes, medium, position );
        extinction += density * ( medium.AbsorptionDensity.xyz + medium.ScatteringPhase.xyz );
        if ( density <= 0.0 || !any( medium.ScatteringPhase.xyz > 0.0 ) ) {
            continue;
        }
        source += density * medium.ScatteringPhase.xyz * ambient;
        for ( uint lightEntry = 0; lightEntry < lightCount; ++lightEntry ) {
            uint lightIndex = overflow ? lightEntry : LoadClusterLight( parameters.Clusters, clusterIndex, lightEntry );
            if ( lightIndex >= parameters.LightCount ) {
                continue;
            }

            LightInfo light = LoadBufferValue<LightInfo>( parameters.Lights, lightIndex * sizeof( LightInfo ) );
            if ( light.ActiveLightType == LIGHT_TYPE_INACTIVE ) {
                continue;
            }
            bool directional = light.ActiveLightType == LIGHT_TYPE_DIRECTIONAL;
            float3 toLight = directional ? -light.Direction.xyz : light.Position.xyz - position;
            float lightDistance = directional ? 1e30 : length( toLight );
            if ( lightDistance <= 1e-6 || dot( toLight, toLight ) <= 1e-12 ) {
                continue;
            }
            float3 incoming = normalize( toLight );
            float attenuation = 1.0;
            if ( !directional ) {
                float window = saturate( 1.0 - pow( lightDistance / max( light.Radius, 1e-6 ), 4.0 ) );
                attenuation = window * window / max( lightDistance * lightDistance, 1e-6 );
                if ( light.ActiveLightType == LIGHT_TYPE_SPOT ) {
                    attenuation *= saturate( ( dot( -incoming, normalize( light.Direction.xyz ) ) - light.OuterCutOff ) /
                                             max( light.CutOff - light.OuterCutOff, 1e-6 ) );
                }
            }
            if ( light.ActiveLightType == LIGHT_TYPE_AREA ) {
                // Dominant-direction approximation for volumetric lighting; surface shading uses the full shape below.
                AreaLightSample area = SampleAreaLight( light, position, incoming, incoming, 1.0 );
                incoming = area.DiffuseDirection;
                attenuation = area.Irradiance;
            }
            if ( attenuation <= 0.0 ) {
                continue;
            }
            float3 visibility = 1.0;
            if ( medium.Flags.w != 0 ) {
                float geometryVisibility = FogGeometryVisibility( light, position, camera );
                if ( geometryVisibility <= 0.0 ) {
                    continue;
                }

                visibility = geometryVisibility * FogLightTransmittance( position, incoming, lightDistance );
            }
            source += density * medium.ScatteringPhase.xyz * light.Diffuse.rgb * light.Intensity * attenuation * visibility *
                      VolumePhaseHG( dot( -direction, incoming ), medium.ScatteringPhase.w );
        }
    }

    if ( frame.Range.w > 0.0 ) {
        float4 clip = mul( frame.PreviousViewProjection, float4( position, 1.0 ) );
        float2 oldUV = float2( clip.x / clip.w * 0.5 + 0.5, 0.5 - clip.y / clip.w * 0.5 );
        if ( clip.w > 0.0 && all( oldUV >= 0.0 ) && all( oldUV <= 1.0 ) ) {
            float3 oldOrigin, oldDirection;
            FogCameraRay( frame.PreviousInverseViewProjection, frame.PreviousCamera.xyz, frame.Frame.y != 0, oldUV, oldOrigin, oldDirection );
            float oldDistance = dot( position - oldOrigin, oldDirection );
            if ( oldDistance > 0.0 && oldDistance < frame.Range.z ) {
                float slice = sqrt( oldDistance / frame.Range.z ) * frame.Grid.z - 0.5;
                source = lerp( source, SampleFogVolume( parameters.History, oldUV, slice, frame.Grid ).rgb, frame.Range.w );
                extinction = lerp( extinction, SampleFogVolume( parameters.ExtinctionHistory, oldUV, slice, frame.Grid ).rgb, frame.Range.w );
            }
        }
    }
    uint2 pixel = FogAtlasPixel( cell, frame.Grid );
    images[parameters.Lighting][pixel] = float4( min( source, 60000.0 ), 0.0 );
    images[parameters.Extinction][pixel] = float4( min( extinction, 60000.0 ), 0.0 );
}
