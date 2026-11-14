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


#include "../../slang/base/Tonemap_Helpers.slang"

struct TonemapParameters {
    float4 ClearColor;
    float Exposure;
    float Gamma;
    int ToneMapper;
#if defined( MKT_SUPPORT_OPENGL )
    uint OutputForPreview;
#endif
};

[[vk::binding( 0, 0 )]] ConstantBuffer<TonemapParameters> parameters : register( b0 );
[[vk::binding( 1, 0 )]] Texture2D<float4> sceneColor : register( t1 );

[shader( "fragment" )]
float4 main( float4 position : SV_Position ) : SV_Target0 {
    int2 scenePixel = int2( position.xy );
#if defined( MKT_SUPPORT_OPENGL )
    if ( parameters.OutputForPreview != 0u ) {
        // ImGui samples the first texture row at the top of the preview.
        // OpenGL's framebuffer origin is at the lower left.
        // https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.60.pdf
        uint width, height;
        sceneColor.GetDimensions( width, height );
        scenePixel.y = int( height ) - 1 - scenePixel.y;
    }
#endif

    float4 hdr = sceneColor.Load( int3( scenePixel, 0 ) );

    // The HDR target accumulates source-over coverage against transparent black.
    // Add the authored display-space clear color where the scene has no coverage.
    if ( hdr.a <= 0.0 ) {
        return parameters.ClearColor;
    }

    float4 mapped = ApplyToneMapping( float4( hdr.rgb / hdr.a, 1.0 ), parameters.ToneMapper, parameters.Exposure, parameters.Gamma );
    return lerp( parameters.ClearColor, mapped, saturate( hdr.a ) );
}
