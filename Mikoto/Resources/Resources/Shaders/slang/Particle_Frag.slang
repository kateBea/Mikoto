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

#pragma once

struct FSInput {
    float4 Position : SV_Position;
    float2 Local : TEXCOORD0;
    float4 Color : COLOR0;
    nointerpolation float Softness : TEXCOORD1;
};

[shader( "fragment" )]
float4 main( FSInput input ) : SV_Target0 {
    float radius = length( input.Local );

    // Procedural soft disc needs no mandatory sprite asset. Derivatives keep
    // the edge smooth when the billboard becomes smaller than a few pixels.
    float feather = max( input.Softness, fwidth( radius ) );
    float coverage = 1.0f - smoothstep( max( 0.0f, 1.0f - feather ), 1.0f, radius );
    float alpha = input.Color.a * coverage;

    if ( alpha <= 0.001f ) {
        discard;
    }

    // The RHI uses SRC_ALPHA / ONE_MINUS_SRC_ALPHA: output straight alpha.
    return float4( input.Color.rgb, alpha );
}
