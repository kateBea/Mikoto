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

// Color interpolation reference; vertex positions come from Mikoto's existing sample.
// https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/shaders.hlsl
// Draw three vertices as a triangle list, starting at vertex 0.

struct VSOutput {
    float4 mPosition : SV_Position;
    float3 mColor : COLOR0;
};

static const float2 kPositions[3] = {
    float2( -0.6, -0.5 ),
    float2( 0.6, -0.5 ),
    float2( 0.0, 0.6 )
};

static const float3 kColors[3] = {
    float3( 1.0, 0.0, 0.0 ),
    float3( 0.0, 1.0, 0.0 ),
    float3( 0.0, 0.0, 1.0 )
};

VSOutput main( uint vertexID : SV_VertexID ) {
    VSOutput output;
    output.mPosition = float4( kPositions[vertexID], 0.0, 1.0 );
    output.mColor = kColors[vertexID];

    return output;
}
