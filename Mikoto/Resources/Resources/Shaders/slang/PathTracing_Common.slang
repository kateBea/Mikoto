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

// Shared ray-scene layouts, hit payloads and indexed-geometry loads. CPU uploads and ray stages
// must preserve these field offsets and index conventions.

#pragma once

#include "base/BufferReference.slang"
#include "base/PushConstants.slang"

// CPU layout: RayTracingModule::FrameConstants. Sharing it also lets closest hit
// reconstruct the intersection from the exact positions used to build the BLAS.
struct FrameParameters {
    float4x4 mInverseViewProjection;
    float4 mCamera;
    uint4 mFrame;// frame number, samples per frame, scattering depth, path mode
    float2 mDisplay;
    uint mOutput;
    uint mSceneIndex;
    uint4 mOptions;// history samples, hybrid shadow samples (zero disables), hybrid lobes, sample/depth cap
    uint64_t mVertices;
    uint64_t mIndices;
    uint64_t mData;
    uint64_t mViewEffects;
    uint mHistory;
    uint mAuxiliary;
    uint mMaterialSampler;
    uint mAtmosphereSampler;
    uint mAtmosphereTextures[3];
    uint mLightDistribution;
    uint64_t mBuildPositions;
    uint mOcclusionOutput;
    uint mOcclusionHistory;
    uint4 mOcclusion;// Enabled, samples, accumulated samples, cosine sampling.
    float mOcclusionMaxDistance;
    uint mVolumePadding;
    uint64_t mVolumes;
    uint64_t mDenoiser;
};

struct PushParameters {
    uint64_t mFrame;
};

MKT_DECLARE_PUSH_CONSTANT_BUFFER( PushParameters, Push );
#define Parameters LoadBufferValue<FrameParameters>( Push.mFrame, 0 )

// Hardware stages provide intersections; raygen implements PBRT's Li() loop.
// https://pbr-book.org/4ed/Light_Transport_I_Surface_Reflection/A_Better_Path_Tracer
static const float RAY_MISS_DISTANCE = -1.0;
static const float RAY_MAX_DISTANCE = 1e30;
static const uint RAY_ALL_INSTANCES = 0xffu;
static const uint MAX_OCCLUSION_HISTORY_SAMPLES = 4096u;

[raypayload] struct Hit {
    float mDistance : read( caller ) : write( caller, closesthit, miss );
    uint mTriangle : read( caller ) : write( caller, closesthit );
    float2 mBarycentrics : read( caller ) : write( caller, closesthit );
    float3 mPosition : read( caller ) : write( closesthit );
    float3 mPositionError : read( caller ) : write( closesthit );
    float3 mGeometricNormal : read( caller ) : write( closesthit );
};

// These records mirror RayTracingModule's packed scene upload.
struct SceneRecord {
    uint mMaterialCount;
    uint mLightCount;
    uint mEmitterCount;
    uint mLightOffset;
    uint mEmitterOffset;
    int mEnvironment;
    uint mFaceOffset;
    uint mHasFaces;
};

struct VertexRecord {
    float4 mPosition;
    float4 mNormal;
    float4 mUV;
    float4 mColor;
};

struct MaterialRecord {
    float4 mBase;
    float4 mEmission;
    float4 mValues;// Emission.w: material=0, front area=1, two-sided area=2
    int4 mMaps;
    float4 mExtra;// normal scale, two sided, pure area emitter, specular workflow
    int4 mUVSets;
    float4 mSpecular;
    float4 mUVTransform;
    uint4 mProperties;
    float4 mOptical;
    float4 mAbsorption;
    float4 mScattering;
    float4 mHeightMapping;
    int4 mHeightInfo;
    float4 mCheckerboardColorA;
    float4 mCheckerboardColorB;
    float4 mCheckerboardOffset;
};

struct LightRecord {
    // Spot.xy: spot cone cosines; Spot.z: PT point radius or directional half-angle (radians).
    float4 mPositionType;
    float4 mDirectionRadius;
    float4 mColorIntensity;
    float4 mSpot;
};

struct EmitterRecord {
    uint mTriangle;
    float mCDF;
    float mArea;
    float mProbability;
};

// Triangle indices and BLAS build positions are packed as three 32-bit values.
// Derive byte offsets from their types so raygen and closest hit agree.
uint3 LoadTriangleIndices( uint triangle ) {
    return LoadBufferValue<uint3>( Parameters.mIndices, triangle * sizeof( uint3 ) );
}

float3 LoadBuildPosition( uint vertex ) {
    return asfloat( LoadBufferValue<uint3>( Parameters.mBuildPositions, vertex * sizeof( uint3 ) ) );
}

MaterialRecord LoadMaterialRecord( uint index ) {
    return LoadBufferValue<MaterialRecord>( Parameters.mData, sizeof( SceneRecord ) + index * sizeof( MaterialRecord ) );
}

// PBRT offsets outside the position's uncertainty interval, then rounds outward.
// https://pbr-book.org/4ed/Shapes/Managing_Rounding_Error
float NextFloat( float value, bool upward ) {
    if ( isinf( value ) && ( upward == ( value > 0.0 ) ) ) {
        return value;
    }
    if ( value == 0.0 ) {
        return asfloat( upward ? 1u : 0x80000001u );
    }

    uint bits = asuint( value );
    return asfloat( upward == ( value > 0.0 ) ? bits + 1u : bits - 1u );
}

float3 OffsetRayOrigin( float3 position, float3 error, float3 normal, float3 direction ) {
    float3 offset = dot( abs( normal ), error ) * normal;
    if ( dot( direction, normal ) < 0.0 ) {
        offset = -offset;
    }

    float3 origin = position + offset;
    for ( uint axis = 0; axis < 3; ++axis ) {
        if ( offset[axis] != 0.0 ) {
            origin[axis] = NextFloat( origin[axis], offset[axis] > 0.0 );
        }
    }
    return origin;
}
