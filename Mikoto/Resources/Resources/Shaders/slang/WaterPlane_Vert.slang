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
#include "base/Water_Helpers.slang"

MKT_DECLARE_PUSH_CONSTANT( WaterParameters, parameters );

[shader("vertex")]
WaterVaryings main( uint vertexID : SV_VertexID ) {
    BufferReference<WaterSurface> surfaces = (BufferReference<WaterSurface>)parameters.surfaces;
    WaterSurface surface = surfaces[parameters.surface];
    BufferReference<uint> indices = (BufferReference<uint>)parameters.indices;
    uint index = indices[surface.geometry.y + vertexID];

    // Rest positions are packed float3s (12 bytes); pull scalars to preserve stride.
    BufferReference<float> vertices = (BufferReference<float>)parameters.vertices;
    uint offset = (surface.geometry.x + index) * 3;
    float3 rest = float3( vertices[offset], vertices[offset + 1], vertices[offset + 2] );
    float3 position, tangentX, tangentZ;
    if ( surface.simulation.z == kWaterSimulationFft ) {
        WaterFftSample sample = GetWaterFftSample( rest.xz, surface.fft.x );
        position = rest + SampleWaterFft( parameters.fftDisplacement, surface.simulation.w, sample, 1 ).xyz;
    }
    else {
        EvaluateWater( rest, surface, parameters.waves, position, tangentX, tangentZ );
    }

    float4 worldPosition = mul( surface.world, float4( position, 1 ) );
    BufferReference<CameraInfo> camera = (BufferReference<CameraInfo>)parameters.camera;

    WaterVaryings output;
    output.position = CameraClipPosition( worldPosition, camera[0] );
    output.worldPosition = worldPosition.xyz;
    output.restPosition = rest.xz;
    // Rest-grid coordinates stick to the moving mesh, including lateral motion.
    output.grid = float2( index % surface.geometry.z, index / surface.geometry.z );
    return output;
}
