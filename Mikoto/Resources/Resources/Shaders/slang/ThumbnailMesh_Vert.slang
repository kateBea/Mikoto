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

#include "base/Thumbnail_Helpers.slang"

[shader( "vertex" )]
PreviewVertex main( uint vertexID : SV_VertexID ) {
    PreviewScene scene = ( ( BufferReference<PreviewScene> )preview.mScene )[0];
    PreviewDraw draw = ( ( BufferReference<PreviewDraw> )preview.mDraws )[preview.mDrawIndex];
    PreviewVertex output;
    VertexDescription vertex;
    if ( draw.mFlags.w > 0.5 ) {
        float2 corners[6] = { float2( -4, -4 ), float2( -4, 4 ), float2( 4, 4 ),
                              float2( -4, -4 ), float2( 4, 4 ),  float2( 4, -4 ) };
        vertex.mPosition = float3( corners[vertexID].x, -0.01, corners[vertexID].y );
        vertex.mNormal = float3( 0, 1, 0 );
        vertex.mUv0 = corners[vertexID];
        vertex.mUv1 = vertex.mUv0;
        vertex.mColor = float4( 1 );
    } else {
        uint index = ( ( BufferReference<uint> )draw.mIndices )[vertexID];
        vertex = ( ( BufferReference<VertexDescription> )draw.mVertices )[index];

        // Match the engine's eight-influence skinning, using a private rest-pose
        // palette. Thumbnail jobs never advance the scene animation clock.
        float4x4 skin = ( float4x4 )0;
        float total = 0;
        for ( uint influence = 0; influence < 8; ++influence ) {
            float joint = influence < 4 ? vertex.mJoints0[influence] : vertex.mJoints1[influence - 4];
            float weight = influence < 4 ? vertex.mWeights0[influence] : vertex.mWeights1[influence - 4];
            if ( weight > 0 && joint >= 0 && joint < draw.mJointCount ) {
                skin += weight * ( ( BufferReference<float4x4> )draw.mPalette )[uint( joint )];
                total += weight;
            }
        }

        if ( total > 1e-6 ) {
            skin /= total;
            vertex.mPosition = mul( skin, float4( vertex.mPosition, 1 ) ).xyz;
            vertex.mNormal = mul( ( float3x3 )skin, vertex.mNormal );
        }
    }

    float4 world = mul( draw.mModel, float4( vertex.mPosition, 1 ) );
    output.mWorld = world.xyz;
    output.mNormal = mul( ( float3x3 )draw.mNormal, vertex.mNormal );
    output.mUv0 = vertex.mUv0;
    output.mUv1 = vertex.mUv1;
    output.mColor = vertex.mColor;
    output.mPosition = mul( preview.mMode == 1 ? scene.mLightViewProjection : scene.mViewProjection, world );
    return output;
}
