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

// Place the water surface using its finite or projected-ocean geometry path. Wave displacement
// and world-space attributes are evaluated before projection.

#include "base/Camera_Helpers.slang"
#include "base/Water_Helpers.slang"

MKT_DECLARE_PUSH_CONSTANT( WaterParameters, parameters );

[shader( "vertex" )]
WaterVaryings main( uint vertexID : SV_VertexID ) {
    BufferReference<WaterSurface> surfaces = ( BufferReference<WaterSurface> )parameters.mSurfaces;
    WaterSurface surface = surfaces[parameters.mSurface];
    BufferReference<uint> indices = ( BufferReference<uint> )parameters.mIndices;
    uint index = indices[surface.mGeometry.y + vertexID];

    // Rest positions are packed float3s (12 bytes); pull scalars to preserve stride.
    BufferReference<float> vertices = ( BufferReference<float> )parameters.mVertices;
    uint offset = ( surface.mGeometry.x + index ) * 3;
    float3 rest = float3( vertices[offset], vertices[offset + 1], vertices[offset + 2] );
    CameraInfo camera = LoadBufferValue<CameraInfo>( parameters.mCamera, 0 );
    float2 grid = float2( index % surface.mGeometry.z, index / surface.mGeometry.z );
    bool ocean = ( surface.mGeometry.w & 64u ) != 0;
    float2 gridSize = float2( surface.mGeometry.z, surface.mCoast.w ) - 1;
    if ( ocean ) {
        rest = WaterProjectedOceanRest( surface, camera, grid / max( gridSize, 1 ) );
    }
    float3 position;
    float3 tangentX;
    float3 tangentZ;
    if ( surface.mSimulation.z == kWaterSimulationFft ) {
        WaterFftSample sample = GetWaterFftSample( rest.xz, surface.mFft.x );
        position = rest + SampleWaterFft( parameters.mFftDisplacement, surface.mSimulation.w, sample, 1 ).xyz;
    }
    else {
        EvaluateWater( rest, surface, parameters.mWaves, position, tangentX, tangentZ );
    }

    float fade = 1;
    if ( ocean ) {
        float3 worldRest = mul( surface.mWorld, float4( rest, 1 ) ).xyz;
        float distance = length( worldRest - camera.mViewPosition.xyz );
        fade = 1 - smoothstep( 80, 800, distance );
        // Fixed borders prevent displaced screen-grid edges opening holes.
        if ( any( grid <= 0 ) || any( grid >= gridSize ) ) {
            fade = 0;
        }
    }
    position = rest + ( position - rest ) * WaterWaveWeight( surface, rest.xz ) * fade;

    float4 worldPosition = mul( surface.mWorld, float4( position, 1 ) );

    WaterVaryings output;
    output.mPosition = CameraClipPosition( worldPosition, camera );
    output.mWorldPosition = worldPosition.xyz;
    output.mRestPosition = rest.xz;
    // Rest-grid coordinates stick to the moving mesh, including lateral motion.
    output.mGrid = ocean ? rest.xz : grid;
    return output;
}
