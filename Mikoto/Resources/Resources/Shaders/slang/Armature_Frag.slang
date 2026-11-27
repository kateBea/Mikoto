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

// Write the armature debug color supplied by the vertex stage. This overlay bypasses material
// lighting.

struct PSInput {
    float4 mPosition : SV_Position;
    float3 mWorldPosition : TEXCOORD0;
    float4 mColor : COLOR0;
};

[shader( "fragment" )]
float4 main( PSInput input ) : SV_Target0 {
    // Flat normals preserve the shape of ozz's bone mesh under nonuniform model scales.
    float3 normal = normalize( cross( ddx( input.mWorldPosition ), ddy( input.mWorldPosition ) ) );
    // ozz's sample ambient shading keeps the facets readable without scene lights.
    // https://github.com/guillaumeblanc/ozz-animation/blob/master/samples/framework/internal/shader.cc#L406
    float3 alpha = ( normal + 1 ) * 0.5f;
    float2 bottomTop = lerp( float2( 0.3f, 0.7f ), float2( 0.4f, 0.8f ), alpha.xz );
    float3 ambient = lerp( float3( bottomTop.x, 0.3f, bottomTop.x ),
        float3( bottomTop.y, 0.8f, bottomTop.y ), alpha.y );
    return float4( input.mColor.rgb * ambient, 1 );
}
