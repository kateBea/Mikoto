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

// Frame-graph example vertex stage: pull the uploaded geometry and forward texture coordinates
// to the sample fragment shader.

#pragma once

struct VSOutput {
    float4 mPosition : SV_Position;
    float2 mTexCoord : ATTRIBUTE0;
    float3 mColor : ATTRIBUTE1;
};

static const float2 positions[4] = { float2( -0.5, -0.5 ), float2( 0.5, -0.5 ), float2( -0.5, 0.5 ),
                                     float2( 0.5, 0.5 ) };

static const float2 texCoords[4] = { float2( 0.0, 0.0 ), float2( 1.0, 0.0 ), float2( 0.0, 1.0 ), float2( 1.0, 1.0 ) };

static const float3 colors[4] = { float3( 1.0, 0.0, 0.0 ), float3( 0.0, 1.0, 0.0 ), float3( 0.0, 0.0, 1.0 ),
                                  float3( 1.0, 1.0, 1.0 ) };

[shader( "vertex" )]
VSOutput main( uint vid : SV_VertexID ) {
    VSOutput o;
    o.mPosition = float4( positions[vid], 0.0, 1.0 );
    o.mTexCoord = texCoords[vid];
    o.mColor = colors[vid];

    return o;
}