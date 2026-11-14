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

#include "Memory_Helpers.slang"
#include "SurfaceCoverage_Helpers.slang"

// ============================================================================
// Constants
// ============================================================================
static const int PBR_WORKFLOW_METALLIC_ROUGHNESS = 0;
static const int PBR_WORKFLOW_SPECULAR_GLOSSINESS = 1;
static const int PBR_WORKFLOW_UNLIT = 2;

static const int PBR_ALPHA_OPAQUE = 0;
static const int PBR_ALPHA_MASK = 1;
static const int PBR_ALPHA_BLEND = 2;

static const int INVALID_UV_SET = -1;
static const int INVALID_TEXTURE_INDEX = -1;

static const float MIN_ROUGHNESS = 0.04;

// ============================================================================
// Structs
// ============================================================================
struct MaterialParameters {
    float4 BaseColorFactor;
    float4 EmissiveFactor;
    float4 DiffuseFactor;
    float4 SpecularFactor;

    float MetallicFactor;
    float RoughnessFactor;
    float AlphaMask;
    float AlphaMaskCutoff;
    float EmissiveStrength;

    int BaseColorTextureSet;
    int MetallicRoughnessTextureSet;
    int SpecularGlossinessSet;
    int NormalTextureSet;
    int OcclusionTextureSet;
    int EmissiveTextureSet;

    int AlbedoIndex;
    int DiffuseIndex;
    int NormalIndex;
    int MetallicIndex;
    int RoughnessIndex;
    int AoIndex;
    int EmissiveIndex;
    int MetallicRoughnessIndex;
    int SpecularGlossinessIndex;

    int IsBloomy;

    int Workflow;

    int mPadding[2];
    float4 Subsurface; // Screen-space scatter scale, weight, shading model, cell bands.
    float4 SurfaceShading; // Coat weight/roughness, toon threshold/softness. Subsurface.w holds cell bands.
    float4 SubsurfaceColorOpacity; // Per-channel scattering weight and suppression; not coverage.
    int ClearCoatNormalIndex;
    int ClearCoatNormalTextureSet;
    float ClearCoatNormalScale;
    float NormalScale;
    float4 SubsurfaceTransmission; // Precomputed near/far attenuation RGB.
    float4 UvTransform;
    float4 HeightMapping; // Scale, midlevel, mode, maximum POM steps.
    int HeightIndex;
    uint HeightUVSet;
    uint2 HeightPadding;
};

struct MeshParameters {
    float4x4 Transform;

    // For vertex pulling, this tells the offset into the  array of vertices
    uint VertexOffset;
    uint IndexOffset;

    int AnimatorID;// MatricesIndex
    int HasArmature;
};

struct VertexDescription {
    float3 position;
    float mPad0;
    float3 normal;
    float mPad1;

    float4 color;

    float2 uv0;
    float2 uv1;

    float4 joints0;
    float4 weights0;

    float4 joints1;
    float4 weights1;
};

// Match renderer::MeshStream in Renderer/Core/MeshRenderData.hh.
// Scoped-enum reference: https://docs.shader-slang.org/en/stable/external/slang/docs/language-reference/types-enum.html
enum class MeshStream : uint {
    Position = 0,
    Normal = 1,
    Color = 2,
    UV0 = 3,
    UV1 = 4,
    Joints0 = 5,
    Weights0 = 6,
    Joints1 = 7,
    Weights1 = 8,
    Count = 9
};

// Each address points to a mesh-local array of one vertex attribute.
struct GeometryAllocation {
    uint64_t mStreams[uint( MeshStream::Count )];
    uint64_t mIndexBuffer;
    uint mAllocationIndex;
    uint mFirstIndex;
    uint mIndexCount;
    uint mPadding;
};

// Persistent GPU instance record.
struct MeshGeometryInfo {
    float4x4 Transform;

    // Index into the buffer
    // of list of skinning matrices
    int SkinningMatricesID;
    int HasArmature;

    // Index into buffer of geometry info
    uint GeometryIndex;

    // TODO: add a list of shadow casters. This will be an index into the array of shadow casters
    // buffer to know from which lights this entity needs shadows casted from, ofc there will be a limit you cannot
    // just slap a unlimited amount of shadow casters

    uint ObjectID;

    uint IsHighlighted;
    uint AlphaMode;
    uint FaceFlags;
    uint MaterialIndex;
    uint64_t SkinnedPositions;
    uint64_t SkinnedNormals;
};

struct SceneGeometryView {
    Pointer Instances;
    Pointer DrawInstances;
};

MeshGeometryInfo LoadMeshGeometry( Pointer address, uint drawInstance ) {
    SceneGeometryView view = LoadBufferValue<SceneGeometryView>( address, 0 );
    uint2 draw = LoadBufferValue<uint2>( view.DrawInstances, drawInstance * 8 );
    MeshGeometryInfo mesh = LoadBufferValue<MeshGeometryInfo>( view.Instances, draw.x * sizeof( MeshGeometryInfo ) );
    mesh.FaceFlags |= draw.y;
    return mesh;
}

// ============================================================================
// Helpers
// ============================================================================º
float4 SRGBtoLINEAR( float4 srgbIn ) {
    float3 linOut = pow( srgbIn.xyz, float3( 2.2 ) );
    return float4( linOut, srgbIn.a );
}

// Match MeshGeometryInfo::mFaceFlags. Mirroring reverses raster winding but
// must not change which side the material considers its outward-facing side.
bool MaterialFrontFacing( uint faceFlags, bool rasterFrontFacing ) {
    return rasterFrontFacing != ( ( faceFlags & 2u ) != 0 );
}

// Raster culling remains disabled for mixed-material indirect batches. Reject
// single-sided backfaces consistently before any pass writes color or depth.
bool MaterialBackfaceDiscard( uint faceFlags, bool rasterFrontFacing ) {
    return ( faceFlags & 1u ) == 0 && !MaterialFrontFacing( faceFlags, rasterFrontFacing );
}

// Opacity stays linear and uses the same texture, UV set, factor, and vertex alpha
// in the depth, selection, and color passes.
int MaterialAlphaTexture( MaterialParameters material ) {
    return material.Workflow == PBR_WORKFLOW_SPECULAR_GLOSSINESS ? material.DiffuseIndex : material.AlbedoIndex;
}

float2 MaterialAlphaUV( MaterialParameters material, float2 uv0, float2 uv1 ) {
    return ( material.BaseColorTextureSet == 0 ? uv0 : uv1 ) * material.UvTransform.xy + material.UvTransform.zw;
}

float MaterialAlpha( MaterialParameters material, float textureAlpha, float vertexAlpha ) {
    float factor = material.Workflow == PBR_WORKFLOW_SPECULAR_GLOSSINESS ? material.DiffuseFactor.a
                                                                         : material.BaseColorFactor.a;
    return saturate( factor * textureAlpha * vertexAlpha );
}

bool MaterialAlphaDiscard( MaterialParameters material, float alpha ) {
    return ( material.AlphaMask == PBR_ALPHA_MASK && alpha < material.AlphaMaskCutoff ) ||
           ( material.AlphaMask == PBR_ALPHA_BLEND && alpha <= 0.0f );
}

// Match the packed PBR pass flag in GeometryShadingModule. Keep the low bits
// available for the environment mip count within the 128-byte push constants.
static const uint PBR_TRANSPARENT_SURFACE_PASS = 1u << 28;

bool MaterialSurfaceDiscard( MaterialParameters material, float alpha, bool transparentPass ) {
    if ( MaterialAlphaDiscard( material, alpha ) ) {
        return true;
    }
    // BLEND describes a material, not every pixel it produces. Its alpha-one
    // pixels must occlude through depth, independently of mesh-centre sorting.
    // Interpolated vertex alpha and filtered texture alpha can round just below
    // one, even for fully opaque source data (e.g. Bistro's BLEND materials).
    // An exact comparison lets depth and color disagree, producing pinholes.
    // Share the same tiny rounding band in both passes; it is far smaller than
    // one 8/16-bit texture-alpha step. Fractional glass remains translucent.
    bool translucent = IsTranslucentBlend( alpha, material.AlphaMask == PBR_ALPHA_BLEND );
    return translucent != transparentPass;
}

// Gets metallic factor from specular glossiness workflow inputs
float ConvertMetallic( float3 diffuse, float3 specular, float maxSpecular ) {
    float perceivedDiffuse =
            sqrt( 0.299 * diffuse.r * diffuse.r + 0.587 * diffuse.g * diffuse.g + 0.114 * diffuse.b * diffuse.b );
    float perceivedSpecular =
            sqrt( 0.299 * specular.r * specular.r + 0.587 * specular.g * specular.g + 0.114 * specular.b * specular.b );
    if ( perceivedSpecular < MIN_ROUGHNESS ) {
        return 0.0;
    }
    float a = MIN_ROUGHNESS;
    float b = perceivedDiffuse * ( 1.0 - maxSpecular ) / ( 1.0 - MIN_ROUGHNESS ) + perceivedSpecular -
              2.0 * MIN_ROUGHNESS;
    float c = MIN_ROUGHNESS - perceivedSpecular;
    float D = max( b * b - 4.0 * a * c, 0.0 );
    return clamp( ( -b + sqrt( D ) ) / ( 2.0 * a ), 0.0, 1.0 );
}

// Inverse square attenuation for point lights (more expensive)
float SphereLightAttenuation( float distance, float radius ) {
    float d2 = distance * distance;
    float r2 = radius * radius;

    return ( 2.0 / r2 ) * ( 1.0 - ( distance / sqrt( d2 + r2 ) ) );
}

// Stream order matches renderer::MeshStream. DrawIndirect invokes the shader once
// per index. Resolve that index once, then pull only the required attributes.
GeometryAllocation GetGeometryAllocation( MeshGeometryInfo mesh, Pointer allocations ) {
    return ( ( BufferReference<GeometryAllocation> )allocations )[mesh.GeometryIndex];
}

uint PullVertexIndex( GeometryAllocation allocation, uint invocation ) {
    return ( ( BufferReference<uint> )allocation.mIndexBuffer )[invocation];
}

float3 PullFloat3( uint64_t address, uint vertexID ) {
    // Scalar loads avoid the 16-byte array stride of a std430 float3 pointer.
    BufferReference<float> values = ( BufferReference<float> )address;
    uint offset = vertexID * 3;
    return float3( values[offset], values[offset + 1], values[offset + 2] );
}

VertexDescription PullPositionVertex( MeshGeometryInfo mesh, GeometryAllocation allocation, uint vertexID ) {
    VertexDescription vertex = ( VertexDescription )0;
    if ( mesh.SkinnedPositions != 0 ) {
        float4 position = ( ( BufferReference<float4> )mesh.SkinnedPositions )[vertexID];
        vertex.position = position.xyz;
        vertex.mPad0 = position.w;
        return vertex;
    }
    vertex.position = PullFloat3( allocation.mStreams[uint( MeshStream::Position )], vertexID );
    // Static meshes omit skinning streams, whose addresses remain zero.
    if ( mesh.SkinningMatricesID > 0 && allocation.mStreams[uint( MeshStream::Joints0 )] != 0 ) {
        vertex.joints0 = ( ( BufferReference<float4> )allocation.mStreams[uint( MeshStream::Joints0 )] )[vertexID];
        vertex.weights0 = ( ( BufferReference<float4> )allocation.mStreams[uint( MeshStream::Weights0 )] )[vertexID];
        vertex.joints1 = ( ( BufferReference<float4> )allocation.mStreams[uint( MeshStream::Joints1 )] )[vertexID];
        vertex.weights1 = ( ( BufferReference<float4> )allocation.mStreams[uint( MeshStream::Weights1 )] )[vertexID];
    }
    return vertex;
}

VertexDescription PullDepthVertex( MeshGeometryInfo mesh, GeometryAllocation allocation, uint vertexID ) {
    VertexDescription vertex = PullPositionVertex( mesh, allocation, vertexID );
    // Keep alpha cutouts identical to shading, without fetching any normals.
    vertex.uv0 = ( ( BufferReference<float2> )allocation.mStreams[uint( MeshStream::UV0 )] )[vertexID];
    vertex.uv1 = ( ( BufferReference<float2> )allocation.mStreams[uint( MeshStream::UV1 )] )[vertexID];
    vertex.color.a = ( ( BufferReference<float> )allocation.mStreams[uint( MeshStream::Color )] )[vertexID * 4 + 3];
    return vertex;
}

VertexDescription PullShadingVertex( MeshGeometryInfo mesh, GeometryAllocation allocation, uint vertexID ) {
    VertexDescription vertex = PullPositionVertex( mesh, allocation, vertexID );
    vertex.normal = mesh.SkinnedNormals != 0 ? ( ( BufferReference<float4> )mesh.SkinnedNormals )[vertexID].xyz
                                             : PullFloat3( allocation.mStreams[uint( MeshStream::Normal )], vertexID );
    vertex.color = ( ( BufferReference<float4> )allocation.mStreams[uint( MeshStream::Color )] )[vertexID];
    vertex.uv0 = ( ( BufferReference<float2> )allocation.mStreams[uint( MeshStream::UV0 )] )[vertexID];
    vertex.uv1 = ( ( BufferReference<float2> )allocation.mStreams[uint( MeshStream::UV1 )] )[vertexID];
    return vertex;
}
