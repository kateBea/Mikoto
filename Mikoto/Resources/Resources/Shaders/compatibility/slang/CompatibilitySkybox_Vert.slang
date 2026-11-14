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

struct SkyInput {
    // Match the position of TEXCOORD0 in the pixel shader's input signature.
    // https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-signatures
    float2 ClipPosition : TEXCOORD0;
    float4 Position : SV_Position;
};

[shader( "vertex" )]
SkyInput main( uint vertexID : SV_VertexID ) {
    SkyInput output;
    output.ClipPosition = float2( ( vertexID << 1 ) & 2, vertexID & 2 ) * 2.0 - 1.0;
    output.Position = float4( output.ClipPosition, 0.0, 1.0 );

    return output;
}
