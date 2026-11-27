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

// Trace short hemisphere rays from visible surfaces to estimate ambient visibility. The AO mode
// shares scene intersection and camera-ray conventions with the path tracer.

#pragma once

// Ambient occlusion estimates visibility with its own progressive history.

// PBRT AOIntegrator::Li: geometric-normal hemisphere visibility, normalized so open space is one.
// https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/cpu/integrators.cpp#L1281
// https://pbrt.org/fileformat-v4#integrators (ambientocclusion: cossample, maxdistance)
float AmbientOcclusion( Surface surface, float3 view, float2 sample, bool cosineSampling,
    float maxDistance, inout uint random ) {
    float3 normal = dot( surface.mGeometricNormal, view ) >= 0.0 ? surface.mGeometricNormal : -surface.mGeometricNormal;
    float3 local;
    float pdf;
    if ( cosineSampling ) {
        float2 disk = SampleLensDisk( sample );
        local = float3( disk, sqrt( max( 0.0, 1.0 - dot( disk, disk ) ) ) );
        pdf = local.z * INV_PI;
    }
    else {
        float sine = sqrt( max( 0.0, 1.0 - sample.x * sample.x ) );
        float phi = 2.0 * PI * sample.y;
        local = float3( sine * cos( phi ), sine * sin( phi ), sample.x );
        pdf = 1.0 / ( 2.0 * PI );
    }
    if ( pdf <= 0.0 ) {
        return 0.0;
    }

    float3 direction = ToWorld( local, normal );
    return Visible( surface, direction, maxDistance, random ) ? max( 0.0, dot( direction, normal ) ) / ( PI * pdf ) : 0.0;
}

void RenderAmbientOcclusion( uint2 pixel ) {
    if ( Parameters.mOcclusion.x == 0 ) {
        return;
    }

    uint previous = Parameters.mOcclusion.z;
    uint count = min( Parameters.mOcclusion.y, MAX_OCCLUSION_HISTORY_SAMPLES - min( previous, MAX_OCCLUSION_HISTORY_SAMPLES ) );
    if ( count == 0 ) {
        return;
    }
    // AO owns its sample stream/history and never perturbs the transport estimator.
    uint random = ( pixel.x + pixel.y * DispatchRaysDimensions().x + 1u ) ^
        ( Parameters.mFrame.x * SAMPLE_STREAMS_PER_FRAME * SAMPLE_SEED_STRIDE ) ^ OCCLUSION_SEED_SALT;
    float3 origin;
    float3 direction;
    GenerateCameraRay( pixel, float2( 0.5 ), origin, direction );
    Surface surface;
    Hit hit;
    float sum = 0.0;
    if ( Intersect( origin, direction, RAY_MAX_DISTANCE, random, surface, hit, true ) ) {
        for ( uint index = 0; index < count; ++index ) {
            float2 sample = Random2D( random );
            sum += AmbientOcclusion( surface, -direction, sample, Parameters.mOcclusion.w != 0,
                Parameters.mOcclusionMaxDistance, random );
        }
    }
    float history = previous > 0 ? RayTextures[Parameters.mOcclusionHistory].Load( int3( pixel, 0 ) ).r : 0.0;
    float average = ( history * float( previous ) + sum ) / float( previous + count );
    RayImages[Parameters.mOcclusionOutput][pixel] = float4( average, average, average, 1.0 );
}
