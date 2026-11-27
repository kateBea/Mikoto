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

// Recombine scattered diffuse illumination with the remaining surface lighting. The MSAA
// variant preserves per-sample surface coverage during composition.

#include "base/Material_Helpers.slang"
#include "base/SubsurfaceCompositeMSAA.slang"
MKT_DECLARE_TEXTURE_SRV( Texture2D ) images;
MKT_DECLARE_SAMPLERS samplers;

[shader( "fragment" )]
float4 main( SubsurfaceCompositeVertex input, bool frontFacing : SV_IsFrontFace ) : SV_Target0 {
    if ( MaterialBackfaceDiscard( input.mFaceFlags, frontFacing ) ) {
        discard;
    }

    int3 pixel = int3( int2( input.mPosition.xy ), 0 );
    float4 original = images[parameters.mOriginal].Load( pixel );
    // A center sample may belong to a different object or have no surface at
    // all. Leave the original PBR samples untouched in those edge pixels.
    if ( original.a != float( input.mMaterialIndex + 1 ) ||
         images[parameters.mMetadata].Load( pixel ).w != float( input.mDrawIdentity ) ) {
             discard;
         }

    MaterialParameters material = ( ( BufferReference<MaterialParameters> )parameters.mMaterials )[input.mMaterialIndex];
    float3 weight = saturate( material.mSubsurfaceColorOpacity.rgb ) *
        saturate( material.mSubsurface.y * ( 1 - material.mSubsurfaceColorOpacity.a ) );
    if ( material.mSubsurface.z != 5.0 || material.mSubsurface.x <= 0 || !any( weight > 0 ) ||
         material.mAlphaMask == PBR_ALPHA_BLEND ) {
             discard;
         }

    // Reapply cutout coverage: depth testing alone could fill alpha holes when
    // a different surface is behind them in one of the MSAA samples.
    if ( material.mAlphaMask == PBR_ALPHA_MASK ) {
        int textureIndex = MaterialAlphaTexture( material );
        float alpha = textureIndex == INVALID_TEXTURE_INDEX ? 1.0 :
            images[NonUniformResourceIndex( textureIndex )].Sample( samplers[parameters.mSampler],
                MaterialAlphaUV( material, input.mUv0, input.mUv1 ) ).a;
        if ( MaterialSurfaceDiscard( material, MaterialAlpha( material, alpha, input.mVertexAlpha ), false ) ) {
            discard;
        }
    }

    float3 diffuse = lerp( original.rgb, images[parameters.mFiltered].Load( pixel ).rgb, weight );
    float3 sharp = images[parameters.mSpecular].Load( pixel ).rgb + images[parameters.mAmbient].Load( pixel ).rgb;

    // The rasterizer/depth attachment select the covered samples. Transparency
    // is rendered afterwards, and only then is the scene color resolved.
    return float4( diffuse + sharp, 1.0 );
}
