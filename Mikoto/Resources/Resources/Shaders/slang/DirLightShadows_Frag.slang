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

// Apply material coverage and sidedness before accepting a shadow-map fragment. This preserves
// cutout holes in the depth map used by light visibility queries.

#pragma once

#include "base/Base.slang"
#include "base/LightInfo.slang"
#include "base/Material_Helpers.slang"
#include "base/Memory_Helpers.slang"
#include "base/ShadowMap_Helpers.slang"

MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;
MKT_DECLARE_PUSH_CONSTANT( ShadowDrawParams, parameters );

[shader( "fragment" )]
float main( ShadowVertexOutput input, bool frontFacing : SV_IsFrontFace ) : SV_Depth {
    if ( MaterialBackfaceDiscard( input.mFaceFlags, frontFacing ) ) {
        discard;
    }

    // Opaque fragments never fetch material data or sample an alpha texture.
    // BLEND assets can contain fully opaque cloth as well as transparent texels.
    // Match the depth prepass: only opaque coverage casts a depth-map shadow.
    if ( input.mAlphaMode != PBR_ALPHA_OPAQUE ) {
        MaterialParameters material =
                ( ( BufferReference<MaterialParameters> )parameters.mMaterials )[input.mMaterialIndex];
        int index = MaterialAlphaTexture( material );
        float alpha = ( index == INVALID_TEXTURE_INDEX ) ? 1.0 :
            textures[NonUniformResourceIndex( index )]
                .Sample( samplers[parameters.mSampler],
                MaterialAlphaUV( material, input.mUv0, input.mUv1 ) ).a;

        if ( MaterialSurfaceDiscard( material, MaterialAlpha( material, alpha, input.mVertexAlpha ), false ) ) {
            discard;
        }
    }

    if ( input.mLightType == LIGHT_TYPE_DIRECTIONAL ) {
        return input.mPosition.z;
    }

    float depth = LocalShadowDepth( input.mWorldPosition, input.mLightPositionRange );

    if ( depth >= 1.0 ) {
        discard;
    }

    return depth;
}
