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


struct ChromaticAberrationParameters {
    float2 TexelSize;
    float Intensity;
    float Direction;
};

[[vk::binding( 0, 0 )]] ConstantBuffer<ChromaticAberrationParameters> parameters : register( b0 );
[[vk::binding( 1, 0 )]] Texture2D<float4> toneMapped : register( t1 );
[[vk::binding( 2, 0 )]] SamplerState linearClamp : register( s2 );

// Adapted from Mikoto's ForwardRenderer shader at ../../slang/ChromaticAberration_Frag.slang.
// Original effect by moonlightoctopus, https://www.shadertoy.com/view/tcKGWw
[shader( "fragment" )]
float4 main( float4 position : SV_Position ) : SV_Target0 {
    float2 uv = position.xy * parameters.TexelSize;
    float2 direction = float2( cos( parameters.Direction ), sin( parameters.Direction ) );
    direction /= max( abs( direction.x ), abs( direction.y ) );

    float2 offset = parameters.TexelSize * parameters.Intensity * direction;
    float2 lower = parameters.TexelSize * 0.5;
    float2 upper = 1.0 - lower;

    float4 red = toneMapped.SampleLevel( linearClamp, clamp( uv, lower, upper ), 0 );
    float green = toneMapped.SampleLevel( linearClamp, clamp( uv - offset, lower, upper ), 0 ).g;
    float blue = toneMapped.SampleLevel( linearClamp, clamp( uv - 2.0 * offset, lower, upper ), 0 ).b;

    return float4( red.r, green, blue, red.a );
}
