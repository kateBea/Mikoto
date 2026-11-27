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

// A four-vertex strip covers the target; noise is evaluated only in the fragment stage.
// Keep UV in register 0 to match the fragment signature on D3D12.
// https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-signatures

// Generate the quad coordinates used by procedural noise passes. The fragment stage maps these
// normalized coordinates into its noise domain.

struct NoiseVaryings {
    float2 mUv : TEXCOORD0;
    float4 mPosition : SV_Position;
};

[shader("vertex")]
NoiseVaryings main( uint vertexIndex : SV_VertexID ) {
    NoiseVaryings output;
    output.mUv = float2( float( vertexIndex & 1 ), float( vertexIndex >> 1 ) );
    output.mPosition = float4( output.mUv * 2.0f - 1.0f, 0.0f, 1.0f );
    return output;
}

