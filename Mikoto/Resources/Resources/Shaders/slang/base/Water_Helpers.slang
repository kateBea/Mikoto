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

// Wave displacement, coast/foam lookup and projected-ocean reconstruction. Surface vertices and
// fragments share these queries to keep water geometry, normals and depth effects aligned.

#pragma once
#include "Camera_Helpers.slang"
#include "Memory_Helpers.slang"
#include "WaterFFT_Helpers.slang"

// Matches WaterWaveGPU/WaterSurfaceGPU in WaterSimulationModule.cc.
struct WaterWave {
    float4 mDirectionAmplitudeFrequency;
    float4 mPhaseHorizontal;
};
struct WaterSurface {
    float4x4 mWorld;
    float4 mColorRoughness;
    float4 mAttenuation;
    float4 mOptics;
    uint4 mGeometry;
    uint4 mSimulation;
    float4 mDebugPreviewColor;
    float4 mFft;// reciprocal patch length
    float4x4 mWorldToLocal;
    float4 mBounds;    // Local width/depth, maximum world depth, animation time.
    float4 mScattering;// Inverse metres, HG anisotropy.
    float4 mCoast;     // Foam depth, amount, wave speed, projected-grid row count.
    float4 mCaustics;  // Strength, cycles per metre, depth range, reserved.
    Pointer mTerrainVertices;
    Pointer mTerrainRecords;
    Pointer mCoastField;
    Pointer mCoastHistory;
    float4 mCoastPatch;   // Local origin XZ, local cell size XZ (one world metre).
    float4 mCoastPrevious;// Previous origin XZ, simulation delta, reset flag.
    float4 mCoastSettings;// Foam lifetime in seconds, full wave depth in metres.
    uint4 mCoastRange;    // First sample, side length.
    uint4 mAirResources;  // Aerial radiance, transmittance, sampler, reserved.
};
struct WaterParameters {
    Pointer mVertices;
    Pointer mIndices;
    Pointer mCamera;
    Pointer mSurfaces;
    Pointer mWaves;
    Pointer mLights;
    uint mSurface;
    uint mLightCount;
    uint mEnvironment;
    uint mIrradiance;
    uint mBrdf;
    uint mSampler;
    float mEnvironmentScale;
    int mToneMap;
    uint mSceneColor;
    uint mSceneDepth;
    uint mScreenSampler;
    Pointer mFftDisplacement;
    Pointer mFftGradients;
    Pointer mShadows;
    uint mShadowCount;
};

struct WaterVaryings {
    float4 mPosition : SV_Position;
    float2 mGrid : TEXCOORD0;
    float3 mWorldPosition : TEXCOORD1;
    float2 mRestPosition : TEXCOORD2;
};

static const uint kWaterSimulationFft = 2;// WaterSimulationType::eFastFourierTransform.

// Periodic bilinear lookup in the FFT output. Coordinates use the same rest plane
// in the vertex and pixel shaders, including when the surface moves horizontally.
struct WaterFftSample {
    uint2 mLower;
    uint2 mUpper;
    float2 mBlend;
};

WaterFftSample GetWaterFftSample( float2 rest, float inversePatchLength ) {
    float2 samplePosition = frac( rest * inversePatchLength ) * kWaterFftResolution;
    WaterFftSample sample;
    sample.mLower = uint2( samplePosition ) & kWaterFftMask;
    sample.mUpper = ( sample.mLower + 1 ) & kWaterFftMask;
    sample.mBlend = frac( samplePosition );
    return sample;
}

float4 SampleWaterFft( Pointer field, uint offset, WaterFftSample sample, uint stride ) {
    BufferReference<float4> values = ( BufferReference<float4> )field;
    float4 a = values[offset + WaterFftIndex( sample.mLower ) * stride];
    float4 b = values[offset + WaterFftIndex( uint2( sample.mUpper.x, sample.mLower.y ) ) * stride];
    float4 c = values[offset + WaterFftIndex( uint2( sample.mLower.x, sample.mUpper.y ) ) * stride];
    float4 d = values[offset + WaterFftIndex( sample.mUpper ) * stride];
    return lerp( lerp( a, b, sample.mBlend.x ), lerp( c, d, sample.mBlend.x ), sample.mBlend.y );
}

// GPU Gems 1, chapter 1: sum directional waves at the REST position. Sampling
// subsequent waves at an already displaced position would change the model.
// Local XZ is the water plane; speed is world units/second before entity scale.
// Evaluate the wave model and its surface derivatives at the rest position. Tangents produce a
// shading normal consistent with the displaced mesh.
void EvaluateWater( float3 rest, WaterSurface surface, Pointer waveAddress,
    out float3 position, out float3 tangentX, out float3 tangentZ ) {
    position = rest;
    tangentX = float3( 1, 0, 0 );
    tangentZ = float3( 0, 0, 1 );
    BufferReference<WaterWave> waves = ( BufferReference<WaterWave> )waveAddress;
    for ( uint i = 0; i < surface.mSimulation.y; ++i ) {
        WaterWave wave = waves[surface.mSimulation.x + i];
        float2 direction = wave.mDirectionAmplitudeFrequency.xy;
        float amplitude = wave.mDirectionAmplitudeFrequency.z;
        float frequency = wave.mDirectionAmplitudeFrequency.w;// 2*pi / wavelength
        float phase = frequency * dot( direction, rest.xz ) + wave.mPhaseHorizontal.x;
        float sine;
        float cosine;
        sincos( phase, sine, cosine );
        position.y += amplitude * sine;
        float slope = amplitude * frequency * cosine;
        tangentX.y += direction.x * slope;
        tangentZ.y += direction.y * slope;

        if ( surface.mSimulation.z == 1 ) {// Gerstner: horizontal crest compression
            float horizontal = wave.mPhaseHorizontal.y;
            position.xz += horizontal * direction * cosine;
            float compression = horizontal * frequency * sine;
            tangentX.xz -= compression * direction.x * direction;
            tangentZ.xz -= compression * direction.y * direction;
        }
    }
}

struct WaterTerrainRecord {
    float4x4 mWorld;
    float4x4 mWorldToLocal;
    uint4 mGrid;
};

// Query the generated terrain rather than a visible screen-depth pixel. Sampling
// the same triangles keeps shorelines stable across camera moves and sculpting.
float WaterBedDepth( WaterSurface surface, float2 rest ) {
    float3 water = mul( surface.mWorld, float4( rest.x, 0, rest.y, 1 ) ).xyz;
    float3 up = normalize( mul( ( float3x3 )surface.mWorld, float3( 0, 1, 0 ) ) );
    float result = surface.mBounds.z;
    if ( surface.mTerrainRecords == 0 || surface.mTerrainVertices == 0 ) {
        return result;
    }
    uint count = LoadBufferValue<uint>( surface.mTerrainRecords, 0 );
    for ( uint i = 0; i < count; ++i ) {
        WaterTerrainRecord terrain = LoadBufferValue<WaterTerrainRecord>( surface.mTerrainRecords, 16 + i * sizeof( WaterTerrainRecord ) );
        // Heightfields are sampled vertically: translation, yaw and scale work;
        // tilted heightfields need a separate general ray/triangle query.
        float3 terrainUp = normalize( mul( ( float3x3 )terrain.mWorld, float3( 0, 1, 0 ) ) );
        if ( dot( terrainUp, up ) < .999 ) {
            continue;
        }
        float3 local = mul( terrain.mWorldToLocal, float4( water, 1 ) ).xyz;
        float4 first = LoadBufferValue<float4>( surface.mTerrainVertices, terrain.mGrid.z * 16 );
        float4 last = LoadBufferValue<float4>( surface.mTerrainVertices, ( terrain.mGrid.z + terrain.mGrid.x - 1 ) * 16 );
        float spacing = ( last.x - first.x ) / max( terrain.mGrid.x - 1, 1 );
        float2 grid = ( local.xz - first.xz ) / max( spacing, 1e-5 );
        if ( any( grid < 0 ) || any( grid > float2( terrain.mGrid.xy - 1 ) ) ) {
            continue;
        }
        uint2 cell = min( uint2( grid ), terrain.mGrid.xy - 2 );
        float2 f = grid - cell;
        uint index = terrain.mGrid.z + cell.y * terrain.mGrid.x + cell.x;
        float4 a = LoadBufferValue<float4>( surface.mTerrainVertices, index * sizeof( float4 ) );
        float4 b = LoadBufferValue<float4>( surface.mTerrainVertices, ( index + 1 ) * 16 );
        float4 c = LoadBufferValue<float4>( surface.mTerrainVertices, ( index + terrain.mGrid.x ) * 16 );
        float4 d = LoadBufferValue<float4>( surface.mTerrainVertices, ( index + terrain.mGrid.x + 1 ) * 16 );
        float3 point = ( f.x + f.y <= 1 ? a + ( b - a ) * f.x + ( c - a ) * f.y : d + ( c - d ) * ( 1 - f.x ) + ( b - d ) * ( 1 - f.y ) ).xyz;
        result = min( result, dot( water - mul( terrain.mWorld, float4( point, 1 ) ).xyz, up ) );
    }
    return result;
}

float WaterWaveWeight( WaterSurface surface, float2 rest ) {
    return smoothstep( 0.0, max( surface.mCoastSettings.y, .1 ), WaterBedDepth( surface, rest ) );
}

// Map the water-local position into the precomputed coast field. Stored depth/shore controls
// are reused for wave attenuation and foam.
float4 WaterCoastLookup( Pointer field, WaterSurface surface, float2 rest, float2 origin ) {
    float2 grid = ( rest - origin ) / surface.mCoastPatch.zw - .5;
    uint side = surface.mCoastRange.y;
    uint2 a = uint2( clamp( floor( grid ), 0.0, float( side - 1 ) ) );
    uint2 b = min( a + 1, side - 1 );
    float2 f = frac( grid );
    BufferReference<float4> values = ( BufferReference<float4> )field;
    uint offset = surface.mCoastRange.x;
    return lerp( lerp( values[offset + a.y * side + a.x], values[offset + a.y * side + b.x], f.x ),
        lerp( values[offset + b.y * side + a.x], values[offset + b.y * side + b.x], f.x ), f.y );
}

float WaterPersistentFoam( WaterSurface surface, float2 rest, float fallback ) {
    float2 grid = ( rest - surface.mCoastPatch.xy ) / surface.mCoastPatch.zw;
    float edge = min( min( grid.x, grid.y ), min( surface.mCoastRange.y - grid.x, surface.mCoastRange.y - grid.y ) );
    if ( edge <= 1 || surface.mCoastField == 0 ) {
        return fallback;
    }
    float foam = WaterCoastLookup( surface.mCoastField, surface, rest, surface.mCoastPatch.xy ).y;
    return lerp( fallback, foam, smoothstep( 1, 16, edge ) );
}

// Adapted from Wicked's oceanSurfaceVS.hlsl/oceanSurfaceHF.hlsli. The screen
// grid follows the view; wave/rest coordinates remain anchored in world space.
// Reconstruct in view space to avoid kilometre-scale ray cancellation.
// Intersect a projected-grid camera ray with the water reference plane. Horizon handling bounds
// the finite mesh used to represent the distant ocean.
float3 WaterProjectedOceanRest( WaterSurface surface, CameraInfo camera, float2 uv ) {
    float2 ndc = ( uv * 2 - 1 ) * 1.15;
    ndc.y = -ndc.y;
    float4 a = mul( camera.mInverseProjection, float4( ndc, .25, 1 ) );
    float4 b = mul( camera.mInverseProjection, float4( ndc, .75, 1 ) );
    float3 pa = a.xyz / a.w;
    float3 pb = b.xyz / b.w;
    bool firstNear = dot( pa, pa ) < dot( pb, pb );
    float3 nearPoint = firstNear ? pa : pb;
    float3 farPoint = firstNear ? pb : pa;
    bool ortho = camera.mProjection[3][3] != 0;
    float3 viewDirection = normalize( ortho ? farPoint - nearPoint : farPoint );
    float3x3 rotation = transpose( ( float3x3 )camera.mViewMatrix );
    float3 direction = normalize( mul( rotation, viewDirection ) );
    float3 origin = camera.mViewPosition.xyz;
    if ( ortho ) {
        origin += mul( rotation, nearPoint - viewDirection * dot( nearPoint, viewDirection ) );
    }
    float3 up = normalize( mul( ( float3x3 )surface.mWorld, float3( 0, 1, 0 ) ) );
    float3 planeOrigin = mul( surface.mWorld, float4( 0, 0, 0, 1 ) ).xyz;
    float denominator = dot( direction, up );
    float height = dot( planeOrigin - origin, up );
    float maximum = camera.mPlanes.y * .98;
    float distance = abs( denominator ) > 1e-7 ? height / denominator : -1;
    float3 world;
    if ( distance > 0 ) {
        world = origin + direction * min( distance, maximum / max( -viewDirection.z, .1 ) );
        // Clamping a distant ray must still leave the vertex on the water plane.
        world += up * dot( planeOrigin - world, up );
    }
    else {
        float3 horizontal = direction - up * denominator;
        horizontal *= rsqrt( max( dot( horizontal, horizontal ), 1e-10 ) );
        world = origin + up * height + horizontal * maximum;
    }
    return mul( surface.mWorldToLocal, float4( world, 1 ) ).xyz;
}
