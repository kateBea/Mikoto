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

// https://bentobaux.github.io/posts/separable-subsurface-scattering-in-unity-6/#3-reconstructing-the-final-image
// https://github.com/bentoBAUX/Separable-Subsurface-Scattering-in-Unity-6
#include "base/Memory_Helpers.slang"
#include "base/Material_Helpers.slang"
struct Parameters {
    Pointer Materials;
    uint Original, Filtered, Specular, Ambient, Scene, Padding;
};

MKT_DECLARE_TEXTURE_SRV( Texture2D ) images;
MKT_DECLARE_PUSH_CONSTANT( Parameters, parameters );

[shader( "fragment" )]
float4 main( float2 unusedUV : TEXCOORD0, float4 position : SV_Position ) : SV_Target0 {
    int3 pixel = int3( int2( position.xy ), 0 );
    float4 scene = images[parameters.Scene].Load( pixel );
    float4 original = images[parameters.Original].Load( pixel );

    // Membership, not luminance: black/shadowed SSS pixels can receive diffusion.
    if ( original.a <= 0 ) {
        return scene;
    }

    MaterialParameters material = ( ( BufferReference<MaterialParameters> )parameters.Materials )[uint( original.a ) - 1];
    float3 weight = saturate( material.SubsurfaceColorOpacity.rgb ) *
        saturate( material.Subsurface.y * ( 1 - material.SubsurfaceColorOpacity.a ) );

    if ( material.Subsurface.x <= 0 || !any( weight > 0 ) ) {
        return scene;
    }

    float3 diffuse = lerp( original.rgb, images[parameters.Filtered].Load( pixel ).rgb, weight );
    float3 sharp = images[parameters.Specular].Load( pixel ).rgb + images[parameters.Ambient].Load( pixel ).rgb;
    return float4( diffuse + sharp, scene.a );
}
