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

#include "base/TerrainRender_Helpers.slang"

// Unlit height visualization using the landscape's configurable elevation colors.
// The normalized source height makes the ramp independent of entity scale.
[shader("fragment")]
float4 main( float height : TEXCOORD0, float2 grid : TEXCOORD1 ) : SV_Target0 {
    float h = saturate( height );
    TerrainElevationColors colors = parameters.elevationColors[0];

    float3 color = h < 0.5f ?
        lerp( colors.low.rgb, colors.middle.rgb, h * 2.0f ) :
        lerp( colors.middle.rgb, colors.high.rgb, ( h - 0.5f ) * 2.0f );

    // Like the water debug view, draw cell edges and vertex dots in pixel units.
    // Both are part of the depth-tested terrain surface; hidden vertices stay hidden.
    const float3 debugColor = float3( 0.0f, 1.0f, 0.0f );
    float2 derivative = max( fwidth( grid ), 1e-5f );
    float2 distance = abs( grid - round( grid ) ) / derivative;
    float visibility = 1.0f - smoothstep( 0.5f, 1.5f, max( derivative.x, derivative.y ) );

    if ( IsDebugView( TerrainDebugFlag::eGridLines ) ) {
        float line = 1.0f - smoothstep( 0.4f, 1.2f, min( distance.x, distance.y ) );
        color = lerp( color, debugColor, line * visibility );
    }

    if ( IsDebugView( TerrainDebugFlag::eVertices ) ) {
        float point = 1.0f - smoothstep( 1.8f, 2.8f, length( distance ) );
        color = lerp( color, debugColor, point * visibility );
    }

    return float4( color, 1.0f );
}
