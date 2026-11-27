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

// Froxel-atlas addressing and fog-light visibility helpers. Slice sampling and local
// transmittance share the camera-volume layout used by lighting and integration.

#pragma once

#include "base/Camera_Helpers.slang"
#include "base/ClusteringLights_Helpers.slang"
#include "base/Fog_Helpers.slang"
#include "base/LightInfo.slang"
#include "base/Memory_Helpers.slang"
#include "base/PushConstants.slang"
#include "base/ShadowMap_Helpers.slang"
#include "base/Volume_Helpers.slang"

// Camera-aligned volume lighting, temporal filtering and front-to-back integration:
// https://github.com/godotengine/godot/blob/master/servers/rendering/renderer_rd/shaders/environment/volumetric_fog_process.glsl
// https://bartwronski.com/wp-content/uploads/2014/08/bwronski_volumetric_fog_siggraph2014.pdf
// The atlas keeps the same froxel organization without adding another RHI image path.
MKT_DECLARE_TEXTURE_SRV( Texture2D<float4> ) textures;
MKT_DECLARE_TEXTURE_SRV( TextureCube<float4> ) cubemaps;
MKT_DECLARE_TEXTURE_UAV( RWTexture2D<float4> ) images;
MKT_DECLARE_SAMPLERS samplers;

struct FogParameters {
    uint64_t mVolumes;
    uint64_t mCamera;
    uint64_t mFrame;
    uint64_t mLights;
    uint64_t mShadows;
    uint64_t mClusters;
    uint mLighting;
    uint mExtinction;
    uint mHistory;
    uint mExtinctionHistory;
    uint mRadiance;
    uint mTransmittance;
    uint mSource;
    uint mDepth;
    uint mSampler;
    uint mLightCount;
    uint mShadowCount;
    uint mStage;
    uint mIrradiance;
    float mAmbientScale;
    float2 mLightGridXY;
    float mLightGridZ;
    uint mClusterCount;
};
// Match the engine's packed CPU constants on D3D12 as well as Vulkan.
MKT_DECLARE_PUSH_CONSTANT( FogParameters, parameters );

uint2 FogAtlasPixel( uint3 cell, uint4 grid ) {
    return cell.xy + uint2( cell.z % grid.w, cell.z / grid.w ) * grid.xy;
}

float4 SampleFogSlice( uint textureID, float2 uv, uint slice, uint4 grid ) {
    return ReadFogSlice( textures[textureID], samplers[parameters.mSampler], uv, slice, grid );
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

#include "base/MediumLighting_Helpers.slang"

float FogGeometryVisibility( LightInfo light, float3 position, CameraInfo camera ) {
    return MediumGeometryVisibility( light, position, camera, parameters.mShadows, parameters.mShadowCount );
}

float3 FogLightTransmittance( float3 origin, float3 direction, float maximum ) {
    return VolumeTransmittance( parameters.mVolumes, origin, direction, maximum );
}
