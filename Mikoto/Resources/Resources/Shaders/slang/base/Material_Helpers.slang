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

// CPU-mirrored material/geometry records and shared vertex pulling. Coverage helpers centralize
// sidedness and alpha tests so picking, depth, shadows and color agree on visible surfaces.

#pragma once

#include "Checkerboard_Helpers.slang"
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
    float4 mBaseColorFactor;
    float4 mEmissiveFactor;
    float4 mDiffuseFactor;
    float4 mSpecularFactor; // RGB specular factor, W glossiness factor.

    float mMetallicFactor;
    float mRoughnessFactor;
    float mAlphaMask;
    float mAlphaMaskCutoff;
    float mEmissiveStrength;

    int mBaseColorTextureSet;
    int mMetallicRoughnessTextureSet;
    int mSpecularGlossinessSet;
    int mNormalTextureSet;
    int mOcclusionTextureSet;
    int mEmissiveTextureSet;

    int mAlbedoIndex;
    int mDiffuseIndex;
    int mNormalIndex;
    int mMetallicIndex;
    int mRoughnessIndex;
    int mAoIndex;
    int mEmissiveIndex;
    int mMetallicRoughnessIndex;
    int mSpecularGlossinessIndex;

    int mIsBloomy;

    int mWorkflow;

    int mPadding[2];
    float4 mSubsurface;            // Screen-space scatter scale, weight, shading model, cell bands.
    float4 mSurfaceShading;        // Coat weight/roughness, toon threshold/softness. Subsurface.w holds cell bands.
    float4 mSubsurfaceColorOpacity;// Per-channel scattering weight and suppression; not coverage.
    int mClearCoatNormalIndex;
    int mClearCoatNormalTextureSet;
    float mClearCoatNormalScale;
    float mNormalScale;
    float4 mSubsurfaceTransmission;// Precomputed near/far attenuation RGB.
    float4 mUvTransform;
    float4 mHeightMapping;// Scale, midlevel, mode, maximum POM steps.
    int mHeightIndex;
    uint mHeightUVSet;
    uint2 mHeightPadding;
    float4 mCheckerboardColorA;
    float4 mCheckerboardColorB;
    float4 mCheckerboardOffset;
};

struct MeshParameters {
    float4x4 mTransform;

    // For vertex pulling, this tells the offset into the  array of vertices
    uint mVertexOffset;
    uint mIndexOffset;

    int mAnimatorID;// MatricesIndex
    int mHasArmature;
};

struct VertexDescription {
    float3 mPosition;
    float mPad0;
    float3 mNormal;
    float mPad1;

    float4 mColor;

    float2 mUv0;
    float2 mUv1;

    float4 mJoints0;
    float4 mWeights0;

    float4 mJoints1;
    float4 mWeights1;
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
    float4x4 mTransform;

    // Index into the buffer
    // of list of skinning matrices
    int mSkinningMatricesID;
    int mHasArmature;

    // Index into buffer of geometry info
    uint mGeometryIndex;

    // TODO: add a list of shadow casters. This will be an index into the array of shadow casters
    // buffer to know from which lights this entity needs shadows casted from, ofc there will be a limit you cannot
    // just slap a unlimited amount of shadow casters

    uint mObjectID;

    uint mIsHighlighted;
    uint mAlphaMode;
    uint mFaceFlags;
    uint mMaterialIndex;
    uint64_t mSkinnedPositions;
    uint64_t mSkinnedNormals;
    float4 mFoliageMotion;
    float4 mFoliageWind;
    float4 mFoliageShape;
};

struct SceneGeometryView {
    Pointer mInstances;
    Pointer mDrawInstances;
};

MeshGeometryInfo LoadMeshGeometry( Pointer address, uint drawInstance ) {
    SceneGeometryView view = LoadBufferValue<SceneGeometryView>( address, 0 );
    uint2 draw = LoadBufferValue<uint2>( view.mDrawInstances, drawInstance * 8 );
    MeshGeometryInfo mesh = LoadBufferValue<MeshGeometryInfo>( view.mInstances, draw.x * sizeof( MeshGeometryInfo ) );
    mesh.mFaceFlags |= draw.y;
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
    return material.mWorkflow == PBR_WORKFLOW_SPECULAR_GLOSSINESS ? material.mDiffuseIndex : material.mAlbedoIndex;
}

float2 MaterialAlphaUV( MaterialParameters material, float2 uv0, float2 uv1 ) {
    return ( material.mBaseColorTextureSet == 0 ? uv0 : uv1 ) * material.mUvTransform.xy + material.mUvTransform.zw;
}

// Combine material alpha factors, the selected UV set and the texture sample. Centralizing this
// query keeps depth/shadow coverage compatible with shaded surfaces.
float MaterialAlpha( MaterialParameters material, float textureAlpha, float vertexAlpha ) {
    float factor = material.mWorkflow == PBR_WORKFLOW_SPECULAR_GLOSSINESS ? material.mDiffuseFactor.a
                                                                         : material.mBaseColorFactor.a;
    return saturate( factor * textureAlpha * vertexAlpha );
}

bool MaterialAlphaDiscard( MaterialParameters material, float alpha ) {
    return ( material.mAlphaMask == PBR_ALPHA_MASK && alpha < material.mAlphaMaskCutoff ) ||
           ( material.mAlphaMask == PBR_ALPHA_BLEND && alpha <= 0.0f );
}

// Match the packed PBR pass flag in GeometryShadingModule. Keep the low bits
// available for the environment mip count within the 128-byte push constants.
static const uint PBR_TRANSPARENT_SURFACE_PASS = 1u << 28;

// Decide whether this fragment belongs to the current opaque/transparent surface pass. Blend
// classification and alpha cutoff are applied before expensive lighting.
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
    bool translucent = IsTranslucentBlend( alpha, material.mAlphaMask == PBR_ALPHA_BLEND );
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
// Resolve the shared geometry allocation for this draw. Vertex-pulling helpers use its offsets
// to address position-only or full shading streams.
GeometryAllocation GetGeometryAllocation( MeshGeometryInfo mesh, Pointer allocations ) {
    return ( ( BufferReference<GeometryAllocation> )allocations )[mesh.mGeometryIndex];
}

// Resolve the draw vertex into the shared index stream. The resulting mesh index is reused by
// the position and shading attribute loads.
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
    if ( mesh.mSkinnedPositions != 0 ) {
        float4 position = ( ( BufferReference<float4> )mesh.mSkinnedPositions )[vertexID];
        vertex.mPosition = position.xyz;
        vertex.mPad0 = position.w;
        return vertex;
    }
    vertex.mPosition = PullFloat3( allocation.mStreams[uint( MeshStream::Position )], vertexID );
    // Static meshes omit skinning streams, whose addresses remain zero.
    if ( mesh.mSkinningMatricesID > 0 && allocation.mStreams[uint( MeshStream::Joints0 )] != 0 ) {
        vertex.mJoints0 = ( ( BufferReference<float4> )allocation.mStreams[uint( MeshStream::Joints0 )] )[vertexID];
        vertex.mWeights0 = ( ( BufferReference<float4> )allocation.mStreams[uint( MeshStream::Weights0 )] )[vertexID];
        vertex.mJoints1 = ( ( BufferReference<float4> )allocation.mStreams[uint( MeshStream::Joints1 )] )[vertexID];
        vertex.mWeights1 = ( ( BufferReference<float4> )allocation.mStreams[uint( MeshStream::Weights1 )] )[vertexID];
    }
    return vertex;
}

// Read only the attributes required by geometry/depth evaluation. Avoiding the full shading
// stream reduces bandwidth in early passes.
VertexDescription PullDepthVertex( MeshGeometryInfo mesh, GeometryAllocation allocation, uint vertexID ) {
    VertexDescription vertex = PullPositionVertex( mesh, allocation, vertexID );
    // Keep alpha cutouts identical to shading, without fetching any normals.
    vertex.mUv0 = ( ( BufferReference<float2> )allocation.mStreams[uint( MeshStream::UV0 )] )[vertexID];
    vertex.mUv1 = ( ( BufferReference<float2> )allocation.mStreams[uint( MeshStream::UV1 )] )[vertexID];
    vertex.mColor.a = ( ( BufferReference<float> )allocation.mStreams[uint( MeshStream::Color )] )[vertexID * 4 + 3];
    return vertex;
}

// Load the complete vertex attributes needed for surface shading. The layout and offsets mirror
// the CPU geometry upload.
VertexDescription PullShadingVertex( MeshGeometryInfo mesh, GeometryAllocation allocation, uint vertexID ) {
    VertexDescription vertex = PullPositionVertex( mesh, allocation, vertexID );
    vertex.mNormal = mesh.mSkinnedNormals != 0 ? ( ( BufferReference<float4> )mesh.mSkinnedNormals )[vertexID].xyz
                                             : PullFloat3( allocation.mStreams[uint( MeshStream::Normal )], vertexID );
    vertex.mColor = ( ( BufferReference<float4> )allocation.mStreams[uint( MeshStream::Color )] )[vertexID];
    vertex.mUv0 = ( ( BufferReference<float2> )allocation.mStreams[uint( MeshStream::UV0 )] )[vertexID];
    vertex.mUv1 = ( ( BufferReference<float2> )allocation.mStreams[uint( MeshStream::UV1 )] )[vertexID];
    return vertex;
}
