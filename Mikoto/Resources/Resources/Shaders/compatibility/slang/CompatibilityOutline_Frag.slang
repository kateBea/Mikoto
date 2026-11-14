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

struct OutlineParameters {
    float4 Color;
    int Radius;
    uint OutputForPreview;
};

[[vk::binding( 0, 0 )]] ConstantBuffer<OutlineParameters> parameters : register( b0 );
[[vk::binding( 1, 0 )]] Texture2D<float4> sceneColor : register( t1 );
[[vk::binding( 3, 0 )]] Texture2D<uint2> selection : register( t3 );

[shader( "fragment" )]
float4 main( float4 position : SV_Position ) : SV_Target0 {
    int2 pixel = int2( position.xy );
    float4 color = sceneColor.Load( int3( pixel, 0 ) );

    uint width, height;
    selection.GetDimensions( width, height );
#if defined( MKT_SUPPORT_OPENGL )
    if ( parameters.OutputForPreview != 0u ) {
        pixel.y = int( height ) - 1 - pixel.y;
    }
#endif

    // Dilate the depth-tested selection mask and draw its outer edge.
    // See the ForwardRenderer's OutlineModule for the equivalent mask and composite passes.
    if ( selection.Load( int3( pixel, 0 ) ).y != 0u ) {
        return color;
    }

    int2 last = int2( width, height ) - 1;
    int radius = parameters.Radius;
    for ( int step = 1; step <= radius; ++step ) {
        for ( int y = -1; y <= 1; ++y ) {
            for ( int x = -1; x <= 1; ++x ) {
                int2 tap = clamp( pixel + step * int2( x, y ), int2( 0 ), last );
                if ( selection.Load( int3( tap, 0 ) ).y != 0u ) {
                    return float4( lerp( color.rgb, parameters.Color.rgb, parameters.Color.a ), color.a );
                }
            }
        }
    }

    return color;
}
