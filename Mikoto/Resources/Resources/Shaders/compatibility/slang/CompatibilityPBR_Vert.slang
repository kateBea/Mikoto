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

#include "base/CompatibilityFrame.slang"
#include "base/CompatibilitySurface.slang"

struct VertexInput {
    [[vk::location( 0 )]] float3 Position : POSITION;
    [[vk::location( 1 )]] float3 Normal : NORMAL;
    [[vk::location( 2 )]] float2 UV : TEXCOORD0;
    [[vk::location( 3 )]] float4 Color : COLOR0;
    [[vk::location( 4 )]] float2 UV1 : TEXCOORD1;
};

[shader( "vertex" )]
SurfaceInput main( VertexInput input ) {
    SurfaceInput output;
    float4 world = mul( draw.World, float4( input.Position, 1.0 ) );

    output.Position = mul( frame.ViewProjection, world );
    output.WorldPosition = world.xyz;
    output.Normal = mul( draw.NormalMatrix, float4( input.Normal, 0.0 ) ).xyz;
    output.UV = input.UV;
    output.UV1 = input.UV1;
    output.Color = input.Color;

    return output;
}
