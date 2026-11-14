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

// Texture/sampler sampling reference, with explicit compatibility bindings:
// https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTexture/shaders.hlsl
// Host contract: texture t0 and sampler s1.
// Explicit SPIR-V bindings preserve distinct texture/sampler slots.
// https://docs.shader-slang.org/en/latest/external/slang/docs/user-guide/a2-01-spirv-target-specific.html

[[vk::binding( 0, 0 )]]
Texture2D<float4> uTexture : register( t0 );

[[vk::binding( 1, 0 )]]
SamplerState uSampler : register( s1 );

struct FSInput {
    float4 mPosition : SV_Position;
    float2 mTexCoord : TEXCOORD0;
};

[shader( "fragment" )]
float4 main( FSInput input ) : SV_Target0 {
    return uTexture.Sample( uSampler, input.mTexCoord );
}
