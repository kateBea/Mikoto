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

// Pull generated terrain positions and project them into opaque scene depth. This early terrain
// depth supplies grass occlusion and contact-shadow receiver reconstruction.

#include "base/Camera_Helpers.slang"
#include "base/TerrainRender_Helpers.slang"

// Match TerrainPlane_Vert's position arithmetic exactly. Lighting can use
// equal-depth terrain surfaces later without seams or a second depth shape.
[shader( "vertex" )]
float4 main( uint vertex : SV_VertexID ) : SV_Position {
    uint local = LoadBufferValue<uint>( parameters.mIndices, ( parameters.mFirstIndex + vertex ) * sizeof( uint ) );
    float3 point = LoadBufferValue<float4>( parameters.mVertices, ( parameters.mFirstVertex + local ) * sizeof( float4 ) ).xyz;
    CameraInfo camera = LoadBufferValue<CameraInfo>( parameters.mCamera, 0 );
    return CameraClipPosition( mul( parameters.mWorld, float4( point, 1 ) ), camera );
}
