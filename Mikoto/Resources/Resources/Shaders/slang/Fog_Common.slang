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

#include "base/Memory_Helpers.slang"
#include "base/PushConstants.slang"
#include "base/Camera_Helpers.slang"
#include "base/Fog_Helpers.slang"
#include "base/Volume_Helpers.slang"
#include "base/LightInfo.slang"
#include "base/ClusteringLights_Helpers.slang"
#include "base/ShadowMap_Helpers.slang"

// Camera-aligned volume lighting, temporal filtering and front-to-back integration:
// https://github.com/godotengine/godot/blob/master/servers/rendering/renderer_rd/shaders/environment/volumetric_fog_process.glsl
// https://bartwronski.com/wp-content/uploads/2014/08/bwronski_volumetric_fog_siggraph2014.pdf
// The atlas keeps the same froxel organization without adding another RHI image path.
MKT_DECLARE_TEXTURE_SRV( Texture2D<float4> )
textures;
MKT_DECLARE_TEXTURE_SRV( TextureCube<float4> )
cubemaps;
MKT_DECLARE_TEXTURE_UAV( RWTexture2D<float4> )
images;
MKT_DECLARE_SAMPLERS samplers;

struct FogParameters {
    uint64_t Volumes, Camera, Frame, Lights, Shadows, Clusters;
    uint Lighting, Extinction, History, ExtinctionHistory;
    uint Radiance, Transmittance, Source, Depth;
    uint Sampler, LightCount, ShadowCount, Stage;
    uint Irradiance;
    float AmbientScale;
    float2 LightGridXY;
    float LightGridZ;
    uint ClusterCount;
};
// Match the engine's packed CPU constants on D3D12 as well as Vulkan.
MKT_DECLARE_PUSH_CONSTANT( FogParameters, parameters );

uint2 FogAtlasPixel( uint3 cell, uint4 grid ) {
    return cell.xy + uint2( cell.z % grid.w, cell.z / grid.w ) * grid.xy;
}

float4 SampleFogSlice( uint textureID, float2 uv, uint slice, uint4 grid ) {
    return ReadFogSlice( textures[textureID], samplers[parameters.Sampler], uv, slice, grid );
}

float4 SampleFogVolume( uint textureID, float2 uv, float slice, uint4 grid ) {
    slice = clamp( slice, 0.0, float( grid.z - 1 ) );
    uint low = uint( floor( slice ) );
    return lerp( SampleFogSlice( textureID, uv, low, grid ),
                 SampleFogSlice( textureID, uv, min( low + 1, grid.z - 1 ), grid ), frac( slice ) );
}

float FogRandom( inout uint state ) {
    state = state * 747796405u + 2891336453u;
    uint value = ( ( state >> ( ( state >> 28u ) + 4u ) ) ^ state ) * 277803737u;
    value = ( value >> 22u ) ^ value;
    return float( value >> 8u ) / 16777216.0;
}

float FogGeometryVisibility( LightInfo light, float3 position, CameraInfo camera ) {
    if ( light.ShadowIndex < 0 || uint( light.ShadowIndex ) >= parameters.ShadowCount ) {
        return 1.0;
    }
    uint offset = uint( light.ShadowIndex ) * sizeof( ShadowRecord );
    ShadowMetadata shadow = LoadBufferValue<ShadowMetadata>( parameters.Shadows, offset + ShadowMetadataOffset );
    uint face = 0;
    if ( light.ActiveLightType == LIGHT_TYPE_DIRECTIONAL ) {
        float depth = abs( mul( camera.mViewMatrix, float4( position, 1.0 ) ).z );
        if ( depth > shadow.CascadeSplits.x ) {
            face = 1;
        }
        if ( depth > shadow.CascadeSplits.y ) {
            face = 2;
        }
        if ( depth > shadow.CascadeSplits.z ) {
            face = 3;
        }
        face = min( face, max( shadow.FaceCount, 1u ) - 1 );
    }
    else if ( light.ActiveLightType == LIGHT_TYPE_POINT ) {
        float3 delta = position - shadow.PositionRange.xyz;
        float3 axis = abs( delta );
        face = axis.x >= max( axis.y, axis.z ) ? ( delta.x >= 0.0 ? 0u : 1u ) : ( axis.y >= axis.z ? ( delta.y >= 0.0 ? 2u : 3u ) : ( delta.z >= 0.0 ? 4u : 5u ) );
    }
    if ( face >= shadow.FaceCount || shadow.TextureID( face ) == 0xffffffffu ) {
        return 1.0;
    }
    float4x4 matrix = LoadBufferValue<float4x4>( parameters.Shadows, offset + face * sizeof( float4x4 ) );
    float4 clip = mul( matrix, float4( position, 1.0 ) );
    if ( clip.w <= 0.0 ) {
        return 1.0;
    }
    float3 ndc = clip.xyz / clip.w;
    float2 uv = float2( ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5 );
    if ( any( uv < 0.0 ) || any( uv > 1.0 ) || ndc.z < 0.0 || ndc.z > 1.0 ) {
        return 1.0;
    }
    float depth = light.ActiveLightType == LIGHT_TYPE_DIRECTIONAL ? ndc.z : LocalShadowDepth( position, shadow.PositionRange );
    float bias = shadow.Bias.x * ( light.ActiveLightType == LIGHT_TYPE_DIRECTIONAL ? length( matrix[2].xyz ) : 1.0 );
    uint textureID = shadow.TextureID( face );
    uint width, height;
    textures[textureID].GetDimensions( width, height );
    int2 pixel = int2( uv * float2( width, height ) - 0.5 );
    float visible = 0.0;
    for ( int y = 0; y < 2; ++y ) {
        for ( int x = 0; x < 2; ++x ) {
            int2 tap = clamp( pixel + int2( x, y ), int2( 0 ), int2( width, height ) - 1 );
            visible += depth - max( bias, 1e-6 ) <= textures[textureID].Load( int3( tap, 0 ) ).r ? 0.25 : 0.0;
        }
    }
    return visible;
}

float3 FogLightTransmittance( float3 origin, float3 direction, float maximum ) {
    return VolumeTransmittance( parameters.Volumes, origin, direction, maximum );
}
