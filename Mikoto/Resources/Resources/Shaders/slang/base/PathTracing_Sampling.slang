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

// Shared transport records, random numbers and local shading coordinates.

static const float PI = 3.141592653589793;
static const float INV_PI = 1.0 / PI;
static const float INV_FOUR_PI = 1.0 / ( 4.0 * PI );
static const uint DIFFUSE = 1u;
static const uint SPECULAR = 2u;
static const uint ALL_LOBES = DIFFUSE | SPECULAR;
// Serialized ShadingModel IDs; preserve the original material enum values.
static const uint MATERIAL_SUBSURFACE = 5u;
static const uint MATERIAL_DIELECTRIC = 6u;
static const uint MATERIAL_CONDUCTOR = 7u;
static const uint MATERIAL_UNLIT = 8u;
static const uint MATERIAL_MEDIUM = 0xfffffffeu; // An interaction inside a volume, not a material model.

// Matches RayTracingModule::Settings::kMaxPathSamplesPerFrame.
static const uint SAMPLE_STREAMS_PER_FRAME = 1024u;

// Hash strides and salts keep pixel, camera-sample and AO streams independent.
static const uint SAMPLE_SEED_STRIDE = 0x9e3779b9u;
static const uint OCCLUSION_SEED_SALT = 0x85ebca6bu;

struct Surface {
    float3 Position, PositionError;
    float3 Normal, GeometricNormal;
    float3 Base, Emission, F0;
    float Roughness, Metallic, Alpha, Cutoff;
    uint AlphaMode;
    bool DoubleSided, AreaLight, EmissionTwoSided, EmissionOnly;
    uint Model, Material, Profile;
    float IOR, Transmission, Thickness, Anisotropy;
    float3 Absorption, Scattering;
    bool FrontFace, ThinWalled, SubsurfaceExit;
};

struct BSDFSample {
    float3 Direction, Value;
    float PDF;
    float Eta;
    bool Delta, Transmitted;
};

struct PreviousVertex {
    float3 Position;
    float BSDFPDF;
    uint LightCount;
};

struct MediumStack {
    uint Materials[8];
    uint Count;
};

struct MediumState {
    float3 Absorption, Scattering;
    float IOR, Anisotropy;
};

SceneRecord SceneData() {
    return LoadBufferValue<SceneRecord>( Parameters.Data, 0 );
}

float MaxComponent( float3 value ) {
    return max( value.x, max( value.y, value.z ) );
}

// PCG's GPU permutation; each pixel sample starts its own deterministic stream.
// https://www.jcgt.org/published/0009/03/02/
// https://pbr-book.org/4ed/Sampling_and_Reconstruction/Independent_Sampler
float Random( inout uint state ) {
    state = state * 747796405u + 2891336453u;
    uint word = ( ( state >> ( ( state >> 28u ) + 4u ) ) ^ state ) * 277803737u;
    word = ( word >> 22u ) ^ word;
    return float( word >> 8u ) * ( 1.0 / 16777216.0 );
}

float2 Random2D( inout uint state ) {
    float x = Random( state );
    float y = Random( state );
    return float2( x, y );
}

// PBRT's shading frame keeps BSDF evaluation and sampling in the same coordinates.
// https://pbr-book.org/4ed/Reflection_Models/BSDF_Representation
float3 Tangent( float3 normal ) {
    return normalize( cross( abs( normal.z ) < 0.99999 ? float3( 0, 0, 1 ) : float3( 0, 1, 0 ), normal ) );
}

float3 ToWorld( float3 local, float3 normal ) {
    float3 tangent = Tangent( normal );
    return tangent * local.x + cross( normal, tangent ) * local.y + normal * local.z;
}

float3 ToLocal( float3 direction, float3 normal ) {
    float3 tangent = Tangent( normal );
    return float3( dot( direction, tangent ), dot( direction, cross( normal, tangent ) ), dot( direction, normal ) );
}

float3 Hemisphere( float3 normal, float cosine, float phi ) {
    float sine = sqrt( max( 0.0, 1.0 - cosine * cosine ) );
    return ToWorld( float3( sine * cos( phi ), sine * sin( phi ), cosine ), normal );
}
