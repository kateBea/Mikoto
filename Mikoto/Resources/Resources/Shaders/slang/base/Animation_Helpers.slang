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

#include "Material_Helpers.slang"

// ============================================================================
// Constants
// ============================================================================
static const uint MAX_BONES = 512;
static const uint MAX_BONE_INFLUENCE = 8;

// ============================================================================
// Structs
// ============================================================================
struct BoneData {
    float4x4 mBoneMatrices[MAX_BONES];
};

struct SkinningResult {
    float4 mPosition;
    float3 mNormal;
};

// Depth-only consumers need neither normal attributes nor normal skinning.
// Keep weight validation, normalization and the unweighted-vertex fallback
// identical to ApplySkinning so their depth matches the shaded surface.
float4 ApplySkinningPosition( VertexDescription vertex, MeshGeometryInfo meshInfo, Pointer skinningBufferPtr ) {
    precise float4 position = float4( vertex.position, meshInfo.SkinnedPositions != 0 ? vertex.mPad0 : 1.0 );
    if ( meshInfo.SkinningMatricesID > 0 && meshInfo.SkinnedPositions == 0 ) {
        BufferReference<float4x4> palettes = ( BufferReference<float4x4> )skinningBufferPtr;
        uint palette = meshInfo.SkinningMatricesID - 1;
        precise float4 accumulated = float4( 0.0 );
        float totalWeight = 0.0;
        for ( uint influence = 0; influence < MAX_BONE_INFLUENCE; ++influence ) {
            float joint = influence < 4 ? vertex.joints0[influence] : vertex.joints1[influence - 4];
            float weight = influence < 4 ? vertex.weights0[influence] : vertex.weights1[influence - 4];
            if ( weight > 0.0 && joint >= 0.0 && joint < MAX_BONES ) {
                accumulated += weight * mul( palettes[palette * MAX_BONES + ( uint )joint], position );
                totalWeight += weight;
            }
        }
        if ( totalWeight > 0.000001 ) {
            position = accumulated / totalWeight;
        }
    }
    precise float4 worldPosition = mul( meshInfo.Transform, position );
    return worldPosition;
}

// Shared deformation math for compute and the vertex fallback. Keep the output
// in mesh space so instances and cameras can reuse it without sharing transforms.
SkinningResult ComputeSkinLocalVertex( VertexDescription vertex, uint paletteIndex, Pointer skinningBufferPtr ) {
    BufferReference<float4x4> palettes = ( BufferReference<float4x4> )skinningBufferPtr;
    float4 position = float4( vertex.position, 1.0 );
    float4 accumulatedPosition = float4( 0.0 );
    float3 accumulatedNormal = float3( 0.0 );
    float totalWeight = 0.0;

    for ( uint influence = 0; influence < MAX_BONE_INFLUENCE; ++influence ) {
        float joint = influence < 4 ? vertex.joints0[influence] : vertex.joints1[influence - 4];
        float weight = influence < 4 ? vertex.weights0[influence] : vertex.weights1[influence - 4];
        if ( weight > 0.0 && joint >= 0.0 && joint < MAX_BONES ) {
            float4x4 bone = palettes[paletteIndex * MAX_BONES + ( uint )joint];
            accumulatedPosition += weight * mul( bone, position );
            accumulatedNormal += weight * mul( ( float3x3 )bone, vertex.normal );
            totalWeight += weight;
        }
    }

    SkinningResult result;
    result.mPosition = position;
    result.mNormal = vertex.normal;
    if ( totalWeight > 0.000001 ) {
        result.mPosition = accumulatedPosition / totalWeight;
        result.mNormal = accumulatedNormal / totalWeight;
    }
    return result;
}

SkinningResult ApplySkinning( VertexDescription vertex, MeshGeometryInfo meshInfo, Pointer skinningBufferPtr ) {
    float4x4 model = meshInfo.Transform;
    float3 normal = vertex.normal;
    if ( meshInfo.SkinningMatricesID > 0 && meshInfo.SkinnedPositions == 0 ) {
        SkinningResult local = ComputeSkinLocalVertex( vertex, meshInfo.SkinningMatricesID - 1, skinningBufferPtr );
        normal = local.mNormal;
    }

    // Same position path as depth/picking, independent of normal math.
    float4 worldPos = ApplySkinningPosition( vertex, meshInfo, skinningBufferPtr );
    float3 worldNormal = mul( ( float3x3 )model, normal );
    // Visibility animation can collapse a mesh to zero scale. Avoid normalizing
    // a zero (or underflowing) vector; scaling first also avoids length overflow.
    float normalMagnitude = max( abs( worldNormal.x ), max( abs( worldNormal.y ), abs( worldNormal.z ) ) );
    worldNormal = normalMagnitude > 0.0 ? normalize( worldNormal / normalMagnitude ) : float3( 0.0, 1.0, 0.0 );

    SkinningResult result;
    result.mPosition = worldPos;
    result.mNormal = worldNormal;

    return result;
}
