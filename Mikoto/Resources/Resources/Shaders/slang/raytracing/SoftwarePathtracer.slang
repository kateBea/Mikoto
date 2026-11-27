// MIT License
//
// Copyright (c) 2026 ケイト
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// GPU port of my CPU pathtracer available at:
// https://github.com/kateBea/RT-In-One-WeekEnd.

// Software intersection and shading used by the compute fallback tracer. Uploaded scene
// primitives are tested directly instead of using a hardware acceleration structure.

#pragma once
#include "../base/Memory_Helpers.slang"

// Trace the demo sphere list in compute using the original CPU transport model.
namespace swrt {
    struct Ray {
        float3 mOrigin;
        float3 mDirection;
    };

    // Matches the upload in CpuPathTraceModule.
    static const uint MATERIAL_DIFFUSE = 0u;
    static const uint MATERIAL_METAL = 1u;
    static const uint MAX_BOUNCES = 50u;
    static const float MIN_HIT_DISTANCE = 0.001;
    static const float MAX_HIT_DISTANCE = 3.402823466e+38;

    struct Sphere {
        float4 mCenterRadius;
        float4 mAlbedoParameter;
        uint mMaterial;
        uint3 mPadding;
    };

    struct Hit {
        float3 mPosition;
        float3 mNormal;
        float3 mAlbedo;
        float mParameter;
        uint mMaterial;
        bool mFrontFace;
    };

    // Independent pixel streams replace the CPU's shared mt19937 ray sampler.
    // A tile's scheduling order never changes its pixels.
    uint NextRandom( inout uint state ) {
        state = state * 747796405u + 2891336453u;
        uint word = ( ( state >> ( ( state >> 28u ) + 4u ) ) ^ state ) * 277803737u;
        return ( word >> 22u ) ^ word;
    }

    float Random( inout uint state ) {
        return float( NextRandom( state ) >> 8 ) * ( 1.0 / 16777216.0 );
    }

    float3 RandomUnitVector( inout uint state ) {
        for ( ;; ) {
            float3 p;
            p.x = 2 * Random( state ) - 1;
            p.y = 2 * Random( state ) - 1;
            p.z = 2 * Random( state ) - 1;
            float lengthSquared = dot( p, p );
            if ( lengthSquared > 1e-30 && lengthSquared <= 1 ) {
                return p * rsqrt( lengthSquared );
            }
        }
    }

    float2 RandomInUnitDisk( inout uint state ) {
        for ( ;; ) {
            float2 p;
            p.x = 2 * Random( state ) - 1;
            p.y = 2 * Random( state ) - 1;
            if ( dot( p, p ) < 1 ) {
                return p;
            }
        }
    }

    bool HitWorld( Ray ray, Pointer address, uint sphereCount, out Hit hit ) {
        BufferReference<Sphere> spheres = ( BufferReference<Sphere> )address;
        float closest = MAX_HIT_DISTANCE;
        uint closestSphere = sphereCount;
        hit = ( Hit )0;
        for ( uint i = 0; i < sphereCount; ++i ) {
            float4 sphere = spheres[i].mCenterRadius;
            float3 oc = sphere.xyz - ray.mOrigin;
            float a = dot( ray.mDirection, ray.mDirection );
            float h = dot( ray.mDirection, oc );
            float c = dot( oc, oc ) - sphere.w * sphere.w;
            float discriminant = h * h - a * c;
            if ( discriminant < 0 ) {
                continue;
            }

            // Try the exit root as well when a ray starts inside the sphere.
            float root = ( h - sqrt( discriminant ) ) / a;
            if ( root <= MIN_HIT_DISTANCE || root >= closest ) {
                root = ( h + sqrt( discriminant ) ) / a;
                if ( root <= MIN_HIT_DISTANCE || root >= closest ) {
                    continue;
                }
            }
            closest = root;
            closestSphere = i;
        }
        if ( closestSphere == sphereCount ) {
            return false;
        }
        Sphere sphere = spheres[closestSphere];
        hit.mPosition = ray.mOrigin + closest * ray.mDirection;
        float3 outwardNormal = ( hit.mPosition - sphere.mCenterRadius.xyz ) / sphere.mCenterRadius.w;
        hit.mFrontFace = dot( ray.mDirection, outwardNormal ) < 0;
        hit.mNormal = hit.mFrontFace ? outwardNormal : -outwardNormal;
        hit.mAlbedo = sphere.mAlbedoParameter.rgb;
        hit.mParameter = sphere.mAlbedoParameter.w;
        hit.mMaterial = sphere.mMaterial;
        return true;
    }

    float3 Refract( float3 direction, float3 normal, float ratio ) {
        float cosine = min( dot( -direction, normal ), 1.0 );
        float3 perpendicular = ratio * ( direction + cosine * normal );
        // abs is deliberate: this is the original vec3::refract implementation.
        return perpendicular - sqrt( abs( 1 - dot( perpendicular, perpendicular ) ) ) * normal;
    }

    float Reflectance( float cosine, float ratio ) {
        float r0 = ( 1 - ratio ) / ( 1 + ratio );
        r0 *= r0;
        return r0 + ( 1 - r0 ) * pow( 1 - cosine, 5 );
    }

    bool Scatter( Ray incoming, Hit hit, inout uint state, out Ray scattered, out float3 attenuation ) {
        scattered.mOrigin = hit.mPosition;
        attenuation = hit.mAlbedo;
        if ( hit.mMaterial == MATERIAL_DIFFUSE ) {
            scattered.mDirection = hit.mNormal + RandomUnitVector( state );
            if ( all( abs( scattered.mDirection ) < 1e-8 ) ) {
                scattered.mDirection = hit.mNormal;
            }
            return true;
        }
        if ( hit.mMaterial == MATERIAL_METAL ) {
            scattered.mDirection = normalize( reflect( incoming.mDirection, hit.mNormal ) ) +
                                  min( hit.mParameter, 1.0 ) * RandomUnitVector( state );
            return dot( scattered.mDirection, hit.mNormal ) > 0;
        }

        attenuation = float3( 1 );
        float ratio = hit.mFrontFace ? 1.0 / hit.mParameter : hit.mParameter;
        float3 direction = normalize( incoming.mDirection );
        float cosine = min( dot( -direction, hit.mNormal ), 1.0 );
        float sine = sqrt( 1 - cosine * cosine );
        bool reflectRay = ratio * sine > 1;
        if ( !reflectRay ) {
            reflectRay = Reflectance( cosine, ratio ) > Random( state );
        }
        scattered.mDirection = reflectRay ? reflect( direction, hit.mNormal ) : Refract( direction, hit.mNormal, ratio );

        // Fidelity note: material.hh overwrites the reflection choice with
        // refraction. Preserve that behavior (including RNG consumption) here.
        scattered.mDirection = Refract( direction, hit.mNormal, ratio );
        return true;
    }

    float3 RayColor( Ray ray, Pointer spheres, uint sphereCount, inout uint state ) {
        float3 throughput = float3( 1 );
        for ( uint depth = 0; depth < MAX_BOUNCES; ++depth ) {
            Hit hit;
            if ( !HitWorld( ray, spheres, sphereCount, hit ) ) {
                float blend = 0.5 * ( normalize( ray.mDirection ).y + 1 );
                return throughput * lerp( float3( 1 ), float3( 0.5, 0.7, 1 ), blend );
            }
            Ray scattered;
            float3 attenuation;
            if ( !Scatter( ray, hit, state, scattered, attenuation ) ) {
                return float3( 0 );
            }
            throughput *= attenuation;
            ray = scattered;
        }
        return float3( 0 );
    }
}
