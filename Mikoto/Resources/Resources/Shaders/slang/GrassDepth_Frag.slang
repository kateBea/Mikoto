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

// Write visible-grass coverage while filling the contact depth target. The material alpha test
// agrees with grass color rendering, including imported mesh cutouts.

#include "base/Grass_Helpers.slang"

MKT_DECLARE_TEXTURE_SRV( Texture2D<float4> ) grassDepthTextures;
MKT_DECLARE_SAMPLERS grassDepthSamplers;

// Use the same authored alpha/UVs as the colour pass. Petal/leaf cutouts
// must remain holes in the contact depth image instead of opaque cards.
// Keep the full vertex semantic order: Slang assigns Vulkan locations densely,
// even when the only colour-pass attribute needed here is shape/UV.
[shader( "fragment" )]
float4 main( float3 world : TEXCOORD0, float3 normal : TEXCOORD1,
    float3 tangent : TEXCOORD2, float4 shape : TEXCOORD3 ) : SV_Target0 {
    GrassDescription source = LoadBufferValue<GrassDescription>( parameters.mDescriptions, parameters.mTerrain * sizeof( GrassDescription ) );
    if ( source.mShadows.x == 0 || source.mShadows.z == 0 ) {
        discard;
    }
    
    if ( source.mMesh.y != 0 && source.mMaterialOptions.y != 0 ) {
        TerrainSurfaceData surface = LoadBufferValue<TerrainSurfaceData>( source.mSurface, 0 );
        float alpha = source.mMaterialColor.a;
        if ( source.mMesh.z != 0xffffffffu ) {
            alpha *= grassDepthTextures[source.mMesh.z].Sample( grassDepthSamplers[surface.mControls.x], shape.yw ).a;
        }
        clip( alpha - source.mMaterialOptions.x );
    }
    
    return saturate( source.mShadows.y );
}
