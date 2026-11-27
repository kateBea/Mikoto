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

// Reconstruct shaded radiance from NRD diffuse/specular signals and their material factors.
// Base alpha marks pixels with valid surface guides; unsupported pixels retain their base
// radiance.

#include "base/Base.slang"
#include "base/Memory_Helpers.slang"
#include "NRD.hlsli"

struct Parameters {
    uint4 mInputs;// diffuse, specular, diffuse factor, specular factor
    uint mBase;
    uint mOutput;
    uint mWidth;
    uint mHeight;
    uint mMethod;
    uint3 mPadding;
};
MKT_DECLARE_TEXTURE_SRV( Texture2D<float4> )
textures;
MKT_DECLARE_TEXTURE_UAV( RWTexture2D<float4> )
images;
MKT_DECLARE_PUSH_CONSTANT( Parameters, parameters );

[shader( "compute" )]
[numthreads( kImageGroupWidth, kImageGroupHeight, 1 )]
void main( uint3 id : SV_DispatchThreadID ) {
    if ( id.x >= parameters.mWidth || id.y >= parameters.mHeight ) {
        return;
    }
    int3 p = int3( id.xy, 0 );
    float4 base = textures[parameters.mBase].Load( p );
    float3 color = base.rgb;
    // Only supported surface pixels reconstruct filtered diffuse/specular signals. The base-
    // image flag preserves unsupported full-radiance estimates through this composition pass.
    if ( base.a > .5 ) {
        float4 diffData = textures[parameters.mInputs.x].Load( p );
        float4 specData = textures[parameters.mInputs.y].Load( p );
        float3 diffuse = parameters.mMethod == 0 ? REBLUR_BackEnd_UnpackRadianceAndNormHitDist( diffData ).rgb : RELAX_BackEnd_UnpackRadiance( diffData ).rgb;
        float3 specular = parameters.mMethod == 0 ? REBLUR_BackEnd_UnpackRadianceAndNormHitDist( specData ).rgb : RELAX_BackEnd_UnpackRadiance( specData ).rgb;
        color += diffuse * textures[parameters.mInputs.z].Load( p ).rgb +
                 specular * textures[parameters.mInputs.w].Load( p ).rgb;
    }
    images[parameters.mOutput][id.xy] = float4( max( color, 0 ), 1 );
}
