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

// Volume layouts, density evaluation and analytic segment integrals. World-space intervals and
// local density coordinates connect authored grids/profiles to light transport.

#pragma once

#include "BufferReference.slang"
#include "CloudFormation_Helpers.slang"
#include "Noise_Helpers.slang"

// Medium data and boundary placement remain separate, following PBRT's MediumInterface.
// https://pbr-book.org/4ed/Volume_Scattering/Media
// CPU layout: Renderer/Passes/VolumeModule.hh; coefficients use inverse world units.
struct VolumeRecord {
    float4x4 mWorldToLocal;
    float4 mAbsorptionDensity;
    float4 mScatteringPhase;
    float4 mDimensionsHeight;
    float4 mNoise;
    uint4 mFlags;
    float4 mWindOctaves;
    uint4 mGrid;
    float4 mExtinctionMajorant;
    uint4 mMajorantGrid;
    uint4 mCloudPattern;
    float4 mCloudProfile;
    float4 mCloudWeather;
};

uint VolumeCount( uint64_t data ) {
    return data != 0 ? LoadBufferValue<uint>( data, 0 ) : 0;
}

VolumeRecord LoadVolume( uint64_t data, uint index ) {
    return LoadBufferValue<VolumeRecord>( data, 16 + index * sizeof( VolumeRecord ) );
}

float VolumePhaseHG( float cosine, float g ) {
    // PBRT's convention: both directions point away from the interaction.
    // https://pbr-book.org/4ed/Volume_Scattering/Phase_Functions
    float denominator = max( 1.0 + g * g + 2.0 * g * cosine, 1e-8 );
    return ( 1.0 - g * g ) / ( 12.566370614359172 * denominator * sqrt( denominator ) );
}

bool VolumeContains( VolumeRecord medium, float3 position ) {
    float3 local = mul( medium.mWorldToLocal, float4( position, 1.0 ) ).xyz;
    return medium.mFlags.x != 0 ? dot( local, local ) <= 0.25 : all( abs( local ) <= 0.5 );
}

// Transform the world ray into volume space and clip it to the supported bounds. The returned
// entry/exit parameters remain distances along the original ray.
bool VolumeRayInterval( VolumeRecord medium, float3 origin, float3 direction, out float entry, out float exit ) {
    float3 o = mul( medium.mWorldToLocal, float4( origin, 1.0 ) ).xyz;
    float3 d = mul( medium.mWorldToLocal, float4( direction, 0.0 ) ).xyz;
    entry = -1e30;
    exit = 1e30;
    if ( medium.mFlags.x != 0 ) {
        float a = dot( d, d );
        float b = dot( o, d );
        float discriminant = b * b - a * ( dot( o, o ) - 0.25 );
        if ( a <= 0.0 || discriminant <= 0.0 ) {
            return false;
        }
        // Stable quadratic roots avoid cancellation for distant, small volumes.
        float q = -b - ( b < 0.0 ? -1.0 : 1.0 ) * sqrt( discriminant );
        float t0 = q / a;
        float t1 = ( dot( o, o ) - 0.25 ) / q;
        entry = min( t0, t1 );
        exit = max( t0, t1 );
        return exit > max( entry, 0.0 );
    }
    for ( uint axis = 0; axis < 3; ++axis ) {
        if ( abs( d[axis] ) < 1e-30 ) {
            if ( abs( o[axis] ) > 0.5 ) {
                return false;
            }
            continue;
        }
        float a = ( -0.5 - o[axis] ) / d[axis];
        float b = ( 0.5 - o[axis] ) / d[axis];
        entry = max( entry, min( a, b ) );
        exit = min( exit, max( a, b ) );
    }
    return exit > max( entry, 0.0 );
}

// Reconstruct density from neighboring grid samples in volume-local coordinates. Grid
// addressing uses the dimensions and offsets of the uploaded volume record.
float VolumeGridValue( uint64_t data, VolumeRecord medium, uint3 voxel ) {
    uint index = voxel.x + medium.mGrid.x * ( voxel.y + medium.mGrid.y * voxel.z );
    return LoadBufferValue<float>( data, medium.mGrid.w + index * sizeof( float ) );
}

// The same rounded cones and lobes as the voxel cloud renderer, evaluated
// analytically for null tracking. Density remains in [0,1], so its existing
// extinction bound stays conservative. No per-frame fluid bake is required.
float VolumeSeededCloudProfile( VolumeRecord medium, float3 local ) {
    float coverage = saturate( medium.mNoise.w );
    if ( coverage <= 0 ) {
        return 0;
    }
    uint count = clamp( medium.mCloudPattern.x, 1u, 64u );
    uint side = uint( ceil( sqrt( float( count ) ) ) );
    float3 dimensions = medium.mDimensionsHeight.xyz;
    float3 extent = dimensions / max( min( dimensions.x, min( dimensions.y, dimensions.z ) ), 1e-6 );
    float3 p = local * extent;
    float spacing = min( extent.x, extent.z ) / side;
    float scale = clamp( medium.mCloudProfile.z, .2, 3 );
    float variation = saturate( medium.mCloudProfile.w );
    float nearest = 1e20;
    float maximumRadius = spacing * .52 * scale * 1.2;
    float maximumReach = maximumRadius * 1.8 + spacing * scale * max( medium.mCloudProfile.y, .001 );

    // Formations live on a seeded XZ lattice. Search only cells whose possible
    // centres can reach this sample, including centre jitter and the outer fringe.
    // This keeps the same density as the full scan without hashing every cloud
    // for each camera/shadow-ray null collision.
    int2 cell = int2( floor( ( local.xz + .5 ) * side ) );
    int search = int( ceil( maximumReach / spacing + .5 + variation * .325 ) );
    int2 first = max( cell - int2( search ), int2( 0 ) );
    int2 last = min( cell + int2( search ), int2( side - 1 ) );
    FBMSettings weatherSettings = { max( medium.mCloudWeather.x, 1e-6 ), 1, .5, 2, 3 };
    for ( int z = first.y; z <= last.y; ++z ) {
        for ( int x = first.x; x <= last.x; ++x ) {
            uint id = uint( x ) + uint( z ) * side;
            if ( id >= count ) {
                continue;
            }
            uint seed = medium.mCloudPattern.y ^ ( id * 911u + 17u );
            float3 random = float3( CloudFormationRandom( seed ), CloudFormationRandom( seed + 1 ), CloudFormationRandom( seed + 2 ) );
            float2 q = float2( x, z );
            float2 center = ( ( q + .5 + ( random.xz - .5 ) * variation * .65 ) / side - .5 ) * extent.xz;
            if ( any( abs( p.xz - center ) > maximumReach ) ) {
                continue;
            }
            float2 weatherPosition = center * max( min( dimensions.x, min( dimensions.y, dimensions.z ) ), 1e-6 );
            float weather = saturate( .5 + .5 * FBM( weatherPosition + float( medium.mCloudPattern.y ), weatherSettings ) );
            float regional = saturate( coverage + ( weather - .5 ) * medium.mCloudWeather.y );
            if ( regional < .02 ) {
                continue;
            }
            float form = clamp( medium.mCloudProfile.x + ( weather - .5 ) * medium.mCloudWeather.z * 2, 0, 3 );
            float radius = spacing * ( .14 + regional * .38 ) * scale * lerp( .8, 1.2, random.x * variation );
            float height = -.08 + ( random.y - .5 ) * .20 * variation * medium.mCloudWeather.w;
            nearest = min( nearest, CloudFormationDistance( p, center, radius, height, form, seed, random ) );
        }
    }

    // Billow noise is bounded by one. No noise/erosion sample can make density
    // nonzero beyond the outer profile, so empty space needs no FBM evaluation.
    if ( nearest > max( spacing * max( medium.mCloudProfile.y, .001 ) * scale, .015 ) ) {
        return 0;
    }
    float3 materialPosition = local * dimensions;
    float seedOffset = float( medium.mCloudPattern.y % 4096u ) * .0137;
    FBMSettings noiseSettings = { max( medium.mNoise.x, 1e-6 ) * .5, 1, .5, 2, uint( medium.mWindOctaves.w ) };
    float billowNoise = saturate( .5 + ( FBM( materialPosition.xy + seedOffset, noiseSettings ) +
        FBM( materialPosition.yz + seedOffset, noiseSettings ) + FBM( materialPosition.zx + seedOffset, noiseSettings ) ) / 6 );
    float profileScale = nearest < 0 ? .14 + coverage * .38 : max( medium.mCloudProfile.y, .001 );
    float shapingScale = max( spacing * profileScale * scale, .015 );
    float normalizedDistance = nearest / shapingScale;
    float billow = lerp( .5, billowNoise, variation );
    float profile = billow < normalizedDistance ? 0 : saturate( billow * ( 1 - normalizedDistance ) );
    float border = smoothstep( 0, .04, .5 - max( abs( local.x ), max( abs( local.y ), abs( local.z ) ) ) );
    noiseSettings.mFrequency = max( medium.mNoise.y, 1e-6 );
    float erosion = saturate( .5 + .5 * FBM( materialPosition.xz + seedOffset, noiseSettings ) );
    return saturate( ( profile - erosion * medium.mNoise.z ) / max( 1 - medium.mNoise.z, .001 ) ) * border;
}

// Select the authored grid or analytic/procedural density profile. All transport queries use
// this shared density definition.
float VolumeDensity( uint64_t data, VolumeRecord medium, float3 position, bool checkBounds = true ) {
    if ( checkBounds && !VolumeContains( medium, position ) ) {
        return 0.0;
    }
    float density = medium.mAbsorptionDensity.w;
    if ( medium.mFlags.y == 1 ) {
        return density * exp( clamp( -medium.mNoise.x * ( position.y - medium.mDimensionsHeight.w ), -80.0, 40.0 ) );
    }
    else if ( medium.mFlags.y == 2 ) {
        if ( medium.mCloudPattern.y != 0 ) {
            float3 local = mul( medium.mWorldToLocal, float4( position, 1 ) ).xyz;
            return density * VolumeSeededCloudProfile( medium, local );
        }
        // Seed zero preserves the existing procedural materials.
        // Reuse the engine's normalized FBM; the final density stays within its majorant.
        // Density belongs to the volume's material frame. Translation, rotation,
        // scale and wind must move the pattern together with its boundary.
        float3 p = mul( medium.mWorldToLocal, float4( position, 1 ) ).xyz * medium.mDimensionsHeight.xyz;
        FBMSettings settings = { medium.mNoise.x, 1.0, 0.5, 2.0, uint( medium.mWindOctaves.w ) };
        float noise = saturate( 0.5 + ( FBM( p.xy, settings ) + FBM( p.yz, settings ) + FBM( p.zx, settings ) ) / 6.0 );
        float coverage = saturate( medium.mNoise.w );
        density *= coverage > 0.0 ? saturate( ( noise - ( 1.0 - coverage ) ) / coverage ) : 0.0;
        settings.mFrequency = medium.mNoise.y;
        density *= saturate( 1.0 - medium.mNoise.z * ( 0.5 + 0.5 * FBM( p.xz, settings ) ) );
    }
    else if ( medium.mFlags.y == 3 ) {
        if ( medium.mGrid.w == 0 || any( medium.mGrid.xyz == 0 ) ) {
            return 0.0;
        }
        float3 local = mul( medium.mWorldToLocal, float4( position, 1.0 ) ).xyz + 0.5;
        float3 voxel = saturate( local ) * float3( medium.mGrid.xyz - 1 );
        uint3 lo = uint3( floor( voxel ) );
        uint3 hi = min( lo + 1, medium.mGrid.xyz - 1 );
        float3 f = frac( voxel );
        float a = lerp( lerp( VolumeGridValue( data, medium, lo ), VolumeGridValue( data, medium, uint3( hi.x, lo.yz ) ), f.x ),
            lerp( VolumeGridValue( data, medium, uint3( lo.x, hi.y, lo.z ) ), VolumeGridValue( data, medium, uint3( hi.xy, lo.z ) ), f.x ), f.y );
        float b = lerp( lerp( VolumeGridValue( data, medium, uint3( lo.xy, hi.z ) ), VolumeGridValue( data, medium, uint3( hi.x, lo.y, hi.z ) ), f.x ),
            lerp( VolumeGridValue( data, medium, uint3( lo.x, hi.yz ) ), VolumeGridValue( data, medium, hi ), f.x ), f.y );
        density *= saturate( lerp( a, b, f.z ) );
    }
    return min( density, medium.mExtinctionMajorant.w );
}

// Analytic height-fog optical depth and inverse follow the plane-parallel
// exponential medium, equations 34/35. Constant fog is its zero-slope limit.
// https://zero-radiance.github.io/post/analytic-media/#exponential-variation-of-density-with-altitude-plane-parallel-atmosphere
// Split only at the authored density clamp, so even a sky ray has at most three pieces.
struct VolumeAnalyticProfile {
    float3 mEnds;
    float mExponent;
    float mSlope;
};

VolumeAnalyticProfile MakeVolumeAnalyticProfile( VolumeRecord medium, float3 origin, float3 direction, float entry, float exit ) {
    VolumeAnalyticProfile profile = ( VolumeAnalyticProfile )0;
    float length = exit - entry;
    profile.mEnds = float3( length );

    if ( medium.mFlags.y == 1 ) {
        profile.mExponent = -medium.mNoise.x * ( origin.y + direction.y * entry - medium.mDimensionsHeight.w );
        profile.mSlope = -medium.mNoise.x * direction.y;

        if ( profile.mSlope != 0.0 ) {
            float2 cuts = clamp( ( float2( -80.0, 40.0 ) - profile.mExponent ) / profile.mSlope, 0.0, length );
            profile.mEnds.xy = float2( min( cuts.x, cuts.y ), max( cuts.x, cuts.y ) );
        }
    }

    return profile;
}

// Construct the piecewise analytic density profile along a clipped ray segment. Later
// integration/sampling can use its closed-form representation.
void VolumeAnalyticSegment( VolumeAnalyticProfile profile, uint index,
    out float start, out float length, out float exponent, out float slope ) {
    start = index == 0 ? 0.0 : profile.mEnds[index - 1];
    length = profile.mEnds[index] - start;
    exponent = clamp( profile.mExponent + profile.mSlope * start, -80.0, 40.0 );

    float middle = profile.mExponent + profile.mSlope * ( start + length * 0.5 );
    slope = middle > -80.0 && middle < 40.0 ? profile.mSlope : 0.0;
}

// Integrate an exponential density segment analytically. The near-zero exponent branch avoids
// numerical cancellation in a nearly constant segment.
float VolumeExponentialIntegral( float exponent, float slope, float length ) {
    float variation = slope * length;

    // The series evaluates expm1(x)/x without cancellation near horizontal rays.
    if ( abs( variation ) < 0.001 ) {
        return exp( exponent ) * length * ( 1.0 + variation * ( 0.5 + variation * ( 1.0 / 6.0 + variation / 24.0 ) ) );
    }

    return max( 0.0, ( exp( clamp( exponent + variation, -80.0, 40.0 ) ) - exp( exponent ) ) / slope );
}

// Invert the analytic segment integral to sample an event distance. Sampling and transmittance
// therefore use matching density profiles.
float VolumeExponentialInverse( float integral, float exponent, float slope ) {
    float distance = integral * exp( -exponent );
    float variation = slope * distance;

    // Likewise evaluate log1p(x)/x without subtracting nearly equal floats.
    if ( abs( variation ) < 0.001 ) {
        return distance * ( 1.0 + variation * ( -0.5 + variation * ( 1.0 / 3.0 - variation / 4.0 ) ) );
    }

    return log( max( 1.0 + variation, 1e-30 ) ) / slope;
}

float VolumeAnalyticIntegral( VolumeAnalyticProfile profile ) {
    float integral = 0.0;

    for ( uint index = 0; index < 3; ++index ) {
        float start;
        float length;
        float exponent;
        float slope;
        VolumeAnalyticSegment( profile, index, start, length, exponent, slope );

        if ( length > 0.0 ) {
            integral += VolumeExponentialIntegral( exponent, slope, length );
        }
    }

    return integral;
}

bool SampleVolumeAnalytic( VolumeAnalyticProfile profile, float integral, out float distance ) {
    distance = profile.mEnds.z;

    for ( uint index = 0; index < 3; ++index ) {
        float start;
        float length;
        float exponent;
        float slope;
        VolumeAnalyticSegment( profile, index, start, length, exponent, slope );

        if ( length <= 0.0 ) {
            continue;
        }

        float segmentIntegral = VolumeExponentialIntegral( exponent, slope, length );

        if ( integral < segmentIntegral ) {
            distance = start + clamp( VolumeExponentialInverse( integral, exponent, slope ), 0.0, length );
            return distance < profile.mEnds.z;
        }

        integral -= segmentIntegral;
    }

    return false;
}

// Evaluate deterministic raster-light attenuation over all intersected volumes. Analytic
// profiles use closed-form integration; heterogeneous density uses midpoint quadrature,
// separately from path-tracing ratio tracking.
// Deterministic raster-lighting quadrature; the path tracer uses separate majorant tracking helpers.
static const uint kVolumeRasterTransmittanceSteps = 16;

float3 VolumeTransmittance( uint64_t data, float3 origin, float3 direction, float maximum ) {
    float3 opticalDepth = 0.0;
    for ( uint index = 0; index < VolumeCount( data ); ++index ) {
        VolumeRecord medium = LoadVolume( data, index );
        float entry;
        float exit;
        if ( medium.mFlags.z == 0 || !VolumeRayInterval( medium, origin, direction, entry, exit ) ) {
            continue;
        }
        entry = max( entry, 0.0 );
        exit = min( exit, maximum );
        if ( exit <= entry ) {
            continue;
        }
        float integral = 0.0;
        if ( medium.mFlags.y <= 1 ) {
            integral = medium.mAbsorptionDensity.w * VolumeAnalyticIntegral(
                                                        MakeVolumeAnalyticProfile( medium, origin, direction, entry, exit ) );
        }
        else {
            float step = ( exit - entry ) / float( kVolumeRasterTransmittanceSteps );
            for ( uint sample = 0; sample < kVolumeRasterTransmittanceSteps; ++sample ) {
                integral += VolumeDensity( data, medium, origin + direction * ( entry + ( sample + 0.5 ) * step ) ) * step;
            }
        }
        opticalDepth += ( medium.mAbsorptionDensity.xyz + medium.mScatteringPhase.xyz ) * integral;
    }
    return exp( -opticalDepth );
}
