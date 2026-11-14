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

#include "base/BufferReference.slang"
#include "base/PushConstants.slang"

// CPU layout: RayTracingModule::FrameConstants. Sharing it also lets closest hit
// reconstruct the intersection from the exact positions used to build the BLAS.
struct FrameParameters {
    float4x4 InverseViewProjection;
    float4 Camera;
    uint4 Frame; // frame number, samples per frame, scattering depth, path mode
    float2 Display;
    uint Output;
    uint SceneIndex;
    uint4 Options; // history samples, hybrid shadows, hybrid lobes, sample/depth cap
    uint64_t Vertices;
    uint64_t Indices;
    uint64_t Data;
    uint64_t ViewEffects;
    uint History;
    uint Auxiliary;
    uint MaterialSampler;
    uint AtmosphereSampler;
    uint AtmosphereTextures[3];
    uint LightDistribution;
    uint64_t BuildPositions;
    uint OcclusionOutput;
    uint OcclusionHistory;
    uint4 Occlusion; // Enabled, samples, accumulated samples, cosine sampling.
    float OcclusionMaxDistance;
    uint VolumePadding;
    uint64_t Volumes;
};

struct PushParameters {
    uint64_t Frame;
};

MKT_DECLARE_PUSH_CONSTANT_BUFFER( PushParameters, Push );
#define Parameters LoadBufferValue<FrameParameters>( Push.Frame, 0 )

// Hardware stages provide intersections; raygen implements PBRT's Li() loop.
// https://pbr-book.org/4ed/Light_Transport_I_Surface_Reflection/A_Better_Path_Tracer
static const float RAY_MISS_DISTANCE = -1.0;
static const float RAY_MAX_DISTANCE = 1e30;
static const uint RAY_ALL_INSTANCES = 0xffu;
static const uint MAX_OCCLUSION_HISTORY_SAMPLES = 4096u;

[raypayload]
struct Hit {
    float Distance : read( caller ) : write( caller, closesthit, miss );
    uint Triangle : read( caller ) : write( caller, closesthit );
    float2 Barycentrics : read( caller ) : write( caller, closesthit );
    float3 Position : read( caller ) : write( closesthit );
    float3 PositionError : read( caller ) : write( closesthit );
    float3 GeometricNormal : read( caller ) : write( closesthit );
};

// These records mirror RayTracingModule's packed scene upload.
struct SceneRecord {
    uint MaterialCount;
    uint LightCount;
    uint EmitterCount;
    uint LightOffset;
    uint EmitterOffset;
    int Environment;
    uint FaceOffset;
    uint HasFaces;
};

struct VertexRecord {
    float4 Position;
    float4 Normal;
    float4 UV;
    float4 Color;
};

struct MaterialRecord {
    float4 Base, Emission, Values; // Emission.w: material=0, front area=1, two-sided area=2
    int4 Maps;
    float4 Extra; // normal scale, two sided, pure area emitter, specular workflow
    int4 UVSets;
    float4 Specular, UVTransform;
    uint4 Properties;
    float4 Optical, Absorption, Scattering;
    float4 HeightMapping;
    int4 HeightInfo;
};

struct LightRecord {
    // Spot.xy: spot cone cosines; Spot.z: PT point radius or directional half-angle (radians).
    float4 PositionType, DirectionRadius, ColorIntensity, Spot;
};

struct EmitterRecord {
    uint Triangle;
    float CDF, Area, Probability;
};

// Triangle indices and BLAS build positions are packed as three 32-bit values.
// Derive byte offsets from their types so raygen and closest hit agree.
uint3 LoadTriangleIndices( uint triangle ) {
    return LoadBufferValue<uint3>( Parameters.Indices, triangle * sizeof( uint3 ) );
}

float3 LoadBuildPosition( uint vertex ) {
    return asfloat( LoadBufferValue<uint3>( Parameters.BuildPositions, vertex * sizeof( uint3 ) ) );
}

MaterialRecord LoadMaterialRecord( uint index ) {
    return LoadBufferValue<MaterialRecord>( Parameters.Data, sizeof( SceneRecord ) + index * sizeof( MaterialRecord ) );
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
