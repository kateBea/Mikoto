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

struct ShadowRecord {
    float4x4 ViewProjection[6];
    float4 CascadeSplits;
    float4 PositionRange;
    uint TextureIDs[6]; // Directional: cascades 0..3, filtered visibility 4, camera receiver depth 5.
    uint FaceCount;
    int LightType;
    // xy: constant/slope bias; directional meters, local normalized radius.
    float4 Bias;
};

// The 80-byte tail of ShadowRecord. Load this first, then fetch only the
// selected matrix from the buffer. Copying the six-matrix array into a local
// record forces dynamically indexed temporary arrays into the pixel shader.
struct ShadowMetadata {
    float4 CascadeSplits;
    float4 PositionRange;
    uint4 TextureIDs0;
    uint2 TextureIDs1;
    uint FaceCount;
    int LightType;
    // xy: constant/slope bias; directional meters, local normalized radius.
    float4 Bias;

    uint TextureID( uint face ) {
        // Scalar selection also avoids materializing a dynamically indexed
        // texture-ID array in the generated shader.
        switch ( face ) {
            case 0: {
                return TextureIDs0.x;
            }
            case 1: {
                return TextureIDs0.y;
            }
            case 2: {
                return TextureIDs0.z;
            }
            case 3: {
                return TextureIDs0.w;
            }
            case 4: {
                return TextureIDs1.x;
            }
            case 5: {
                return TextureIDs1.y;
            }
            default: {
                return 0xffffffffu;
            }
        }
    }
};

static const uint ShadowMetadataOffset = 6 * sizeof( float4x4 );
struct ShadowDrawParams {
    uint64_t Geometry;
    uint64_t Skinning;
    uint64_t Allocation;
    uint64_t Records;
    uint64_t Materials;
    uint Sampler;
    uint Record;
    uint Face;
    uint Padding;
};

struct ShadowVertexOutput {
    float4 Position : SV_Position;
    nointerpolation uint MaterialIndex : TEXCOORD0;
    float2 Uv0 : TEXCOORD1;
    float2 Uv1 : TEXCOORD2;
    float VertexAlpha : TEXCOORD3;
    float3 WorldPosition : TEXCOORD4;
    nointerpolation float4 LightPositionRange : TEXCOORD5;
    nointerpolation int LightType : TEXCOORD6;
    nointerpolation uint AlphaMode : TEXCOORD7;
    nointerpolation uint FaceFlags : TEXCOORD8;
};

// Perspective z compresses most of a local light's range near 1. A fixed
// bias in that space can skip entire small casters. Store and compare radial
// distance for both point and spot lights so bias stays linear in light range.
float LocalShadowDepth( float3 world, float4 positionRange ) {
    return length( world - positionRange.xyz ) / max( positionRange.w, 0.000001 );
}

// Orthographic cascades have orthogonal, scaled world-to-NDC rows. Express
// the receiver's geometric plane in those coordinates and evaluate z at the
// actual texel center, including the center tap's subtexel displacement.
// Reusing the original z for every PCF tap makes a sloping plane shadow itself.
float DirectionalShadowReceiverDepth( float4x4 viewProjection, float2 texelUV, float3 receiverNdc, float3 normal ) {
    float3 right = viewProjection[0].xyz;
    float3 up = viewProjection[1].xyz;
    float3 depth = viewProjection[2].xyz;
    float denominator = dot( normal, depth );
    // A plane parallel to the light rays has no unique receiver intersection.
    if ( abs( denominator ) <= 1e-5 * length( normal ) * length( depth ) ) {
        return -1.0;
    }
    float3 plane = float3( dot( normal, right ) / dot( right, right ), dot( normal, up ) / dot( up, up ),
                           denominator / dot( depth, depth ) );
    float2 ndc = float2( texelUV.x * 2.0 - 1.0, 1.0 - texelUV.y * 2.0 );
    return receiverNdc.z - dot( plane.xy, ndc - receiverNdc.xy ) / plane.z;
}

// Compare at the texel center on the receiver's triangle plane. A PCF tap
// moves along that plane, not along a sphere at the original receiver distance.
// Reconstruct the perspective ray from the orthogonal rows of projection*view;
// this works for our symmetric point/spot projections without a matrix inverse.
float LocalShadowReceiverDepth( float4x4 viewProjection, float2 texelUV, float3 receiver, float3 normal,
                                float4 positionRange ) {
    float3 lightToReceiver = receiver - positionRange.xyz;
    float normalLength = length( normal );
    float planeDistance = dot( normal, lightToReceiver );
    // At contact the light lies on the receiver plane. Interpolated positions
    // and screen-space derivatives have float rounding error, which can change
    // the sign of this distance between triangles. Dividing it by a grazing
    // ray magnifies that error into large, angular false shadows. Treat distances
    // within float precision as coplanar, independent of the PCF ray direction.
    float planeTolerance = 1e-6 * normalLength * max( length( lightToReceiver ), 1e-6 );
    if ( normalLength <= 0.0 ||
         abs( planeDistance ) <= planeTolerance ) {
        return -1.0;
    }
    float2 ndc = float2( texelUV.x * 2.0 - 1.0, 1.0 - texelUV.y * 2.0 );
    float3 right = viewProjection[0].xyz;
    float3 up = viewProjection[1].xyz;
    float3 forward = viewProjection[3].xyz;
    float3 ray = forward + right * ( ndc.x / dot( right, right ) ) + up * ( ndc.y / dot( up, up ) );
    float denominator = dot( normal, ray );
    // A tap parallel to the plane or behind the light has no receiver.
    if ( abs( denominator ) <= 0.00001 * normalLength * length( ray ) ) {
        return -1.0;
    }
    float t = planeDistance / denominator;
    if ( t <= 0.0 ) {
        return -1.0;
    }
    return length( ray ) * t / max( positionRange.w, 0.000001 );
}

uint PointShadowFace( float3 direction ) {
    float3 a = abs( direction );
    if ( a.x >= a.y && a.x >= a.z ) {
        return direction.x >= 0 ? 0 : 1;
    }
    if ( a.y >= a.z ) {
        return direction.y >= 0 ? 2 : 3;
    }
    return direction.z >= 0 ? 4 : 5;
}
