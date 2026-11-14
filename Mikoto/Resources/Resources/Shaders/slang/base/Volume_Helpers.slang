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

#include "BufferReference.slang"
#include "Noise_Helpers.slang"

// Medium data and boundary placement remain separate, following PBRT's MediumInterface.
// https://pbr-book.org/4ed/Volume_Scattering/Media
// CPU layout: Renderer/Passes/VolumeModule.hh; coefficients use inverse world units.
struct VolumeRecord {
    float4x4 WorldToLocal;
    float4 AbsorptionDensity;
    float4 ScatteringPhase;
    float4 DimensionsHeight;
    float4 Noise;
    uint4 Flags;
    float4 WindOctaves;
    uint4 Grid;
    float4 ExtinctionMajorant;
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
    float3 local = mul( medium.WorldToLocal, float4( position, 1.0 ) ).xyz;
    return medium.Flags.x != 0 ? dot( local, local ) <= 0.25 : all( abs( local ) <= 0.5 );
}

bool VolumeRayInterval( VolumeRecord medium, float3 origin, float3 direction, out float entry, out float exit ) {
    float3 o = mul( medium.WorldToLocal, float4( origin, 1.0 ) ).xyz;
    float3 d = mul( medium.WorldToLocal, float4( direction, 0.0 ) ).xyz;
    entry = -1e30;
    exit = 1e30;
    if ( medium.Flags.x != 0 ) {
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

float VolumeGridValue( uint64_t data, VolumeRecord medium, uint3 voxel ) {
    uint index = voxel.x + medium.Grid.x * ( voxel.y + medium.Grid.y * voxel.z );
    return LoadBufferValue<float>( data, medium.Grid.w + index * 4 );
}

float VolumeDensity( uint64_t data, VolumeRecord medium, float3 position, bool checkBounds = true ) {
    if ( checkBounds && !VolumeContains( medium, position ) ) {
        return 0.0;
    }
    float density = medium.AbsorptionDensity.w;
    if ( medium.Flags.y == 1 ) {
        return density * exp( clamp( -medium.Noise.x * ( position.y - medium.DimensionsHeight.w ), -80.0, 40.0 ) );
    }
    else if ( medium.Flags.y == 2 ) {
        // Reuse the engine's normalized FBM; the final density stays within its majorant.
        float3 p = position - medium.WindOctaves.xyz;
        FBMSettings settings = { medium.Noise.x, 1.0, 0.5, 2.0, uint( medium.WindOctaves.w ) };
        float noise = saturate( 0.5 + ( FBM( p.xy, settings ) + FBM( p.yz, settings ) + FBM( p.zx, settings ) ) / 6.0 );
        float coverage = saturate( medium.Noise.w );
        density *= coverage > 0.0 ? saturate( ( noise - ( 1.0 - coverage ) ) / coverage ) : 0.0;
        settings.mFrequency = medium.Noise.y;
        density *= saturate( 1.0 - medium.Noise.z * ( 0.5 + 0.5 * FBM( p.xz, settings ) ) );
    }
    else if ( medium.Flags.y == 3 ) {
        if ( medium.Grid.w == 0 || any( medium.Grid.xyz == 0 ) ) {
            return 0.0;
        }
        float3 local = mul( medium.WorldToLocal, float4( position, 1.0 ) ).xyz + 0.5;
        float3 voxel = saturate( local ) * float3( medium.Grid.xyz - 1 );
        uint3 lo = uint3( floor( voxel ) );
        uint3 hi = min( lo + 1, medium.Grid.xyz - 1 );
        float3 f = frac( voxel );
        float a = lerp( lerp( VolumeGridValue( data, medium, lo ), VolumeGridValue( data, medium, uint3( hi.x, lo.yz ) ), f.x ),
                        lerp( VolumeGridValue( data, medium, uint3( lo.x, hi.y, lo.z ) ), VolumeGridValue( data, medium, uint3( hi.xy, lo.z ) ), f.x ), f.y );
        float b = lerp( lerp( VolumeGridValue( data, medium, uint3( lo.xy, hi.z ) ), VolumeGridValue( data, medium, uint3( hi.x, lo.y, hi.z ) ), f.x ),
                        lerp( VolumeGridValue( data, medium, uint3( lo.x, hi.yz ) ), VolumeGridValue( data, medium, hi ), f.x ), f.y );
        density *= saturate( lerp( a, b, f.z ) );
    }
    return min( density, medium.ExtinctionMajorant.w );
}

// Analytic height-fog optical depth and inverse follow the plane-parallel
// exponential medium, equations 34/35. Constant fog is its zero-slope limit.
// https://zero-radiance.github.io/post/analytic-media/#exponential-variation-of-density-with-altitude-plane-parallel-atmosphere
// Split only at the authored density clamp, so even a sky ray has at most three pieces.
struct VolumeAnalyticProfile {
    float3 Ends;
    float Exponent;
    float Slope;
};

VolumeAnalyticProfile MakeVolumeAnalyticProfile( VolumeRecord medium, float3 origin, float3 direction, float entry, float exit ) {
    VolumeAnalyticProfile profile = ( VolumeAnalyticProfile )0;
    float length = exit - entry;
    profile.Ends = float3( length );

    if ( medium.Flags.y == 1 ) {
        profile.Exponent = -medium.Noise.x * ( origin.y + direction.y * entry - medium.DimensionsHeight.w );
        profile.Slope = -medium.Noise.x * direction.y;

        if ( profile.Slope != 0.0 ) {
            float2 cuts = clamp( ( float2( -80.0, 40.0 ) - profile.Exponent ) / profile.Slope, 0.0, length );
            profile.Ends.xy = float2( min( cuts.x, cuts.y ), max( cuts.x, cuts.y ) );
        }
    }

    return profile;
}

void VolumeAnalyticSegment( VolumeAnalyticProfile profile, uint index,
    out float start, out float length, out float exponent, out float slope ) {
    start = index == 0 ? 0.0 : profile.Ends[index - 1];
    length = profile.Ends[index] - start;
    exponent = clamp( profile.Exponent + profile.Slope * start, -80.0, 40.0 );

    float middle = profile.Exponent + profile.Slope * ( start + length * 0.5 );
    slope = middle > -80.0 && middle < 40.0 ? profile.Slope : 0.0;
}

float VolumeExponentialIntegral( float exponent, float slope, float length ) {
    float variation = slope * length;

    // The series evaluates expm1(x)/x without cancellation near horizontal rays.
    if ( abs( variation ) < 0.001 ) {
        return exp( exponent ) * length * ( 1.0 + variation * ( 0.5 + variation * ( 1.0 / 6.0 + variation / 24.0 ) ) );
    }

    return max( 0.0, ( exp( clamp( exponent + variation, -80.0, 40.0 ) ) - exp( exponent ) ) / slope );
}

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
        float start, length, exponent, slope;
        VolumeAnalyticSegment( profile, index, start, length, exponent, slope );

        if ( length > 0.0 ) {
            integral += VolumeExponentialIntegral( exponent, slope, length );
        }
    }

    return integral;
}

bool SampleVolumeAnalytic( VolumeAnalyticProfile profile, float integral, out float distance ) {
    distance = profile.Ends.z;

    for ( uint index = 0; index < 3; ++index ) {
        float start, length, exponent, slope;
        VolumeAnalyticSegment( profile, index, start, length, exponent, slope );

        if ( length <= 0.0 ) {
            continue;
        }

        float segmentIntegral = VolumeExponentialIntegral( exponent, slope, length );

        if ( integral < segmentIntegral ) {
            distance = start + clamp( VolumeExponentialInverse( integral, exponent, slope ), 0.0, length );
            return distance < profile.Ends.z;
        }

        integral -= segmentIntegral;
    }

    return false;
}

float3 VolumeTransmittance( uint64_t data, float3 origin, float3 direction, float maximum ) {
    float3 opticalDepth = 0.0;
    for ( uint index = 0; index < VolumeCount( data ); ++index ) {
        VolumeRecord medium = LoadVolume( data, index );
        float entry, exit;
        if ( medium.Flags.z == 0 || !VolumeRayInterval( medium, origin, direction, entry, exit ) ) {
            continue;
        }
        entry = max( entry, 0.0 );
        exit = min( exit, maximum );
        if ( exit <= entry ) {
            continue;
        }
        float integral = 0.0;
        if ( medium.Flags.y <= 1 ) {
            integral = medium.AbsorptionDensity.w * VolumeAnalyticIntegral(
                MakeVolumeAnalyticProfile( medium, origin, direction, entry, exit ) );
        }
        else {
            float step = ( exit - entry ) / 16.0;
            for ( uint sample = 0; sample < 16; ++sample ) {
                integral += VolumeDensity( data, medium, origin + direction * ( entry + ( sample + 0.5 ) * step ) ) * step;
            }
        }
        opticalDepth += ( medium.AbsorptionDensity.xyz + medium.ScatteringPhase.xyz ) * integral;
    }
    return exp( -opticalDepth );
}
