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

// Traverse volume majorants and sample candidate free-flight events. Ratio tracking estimates
// transmittance without forcing a fixed small step through every empty region.

#pragma once

#include "Volume_Helpers.slang"

// Piecewise-constant conservative majorants, following PBRT's SampleRay/DDA.
// https://pbr-book.org/4ed/Volume_Scattering/Media
// https://alextardif.com/ParticipatingMedia.html
// Random(inout uint) is supplied by the caller's independent sampling stream.
struct VolumeMajorantIterator {
    float3 mCrossing;
    float3 mDelta;
    int3 mCell;
    int3 mAdvance;
    float mTime;
    float mEnd;
    uint mRemaining;
    bool mGrid;
};

// Initialize grid DDA traversal for the ray interval inside this volume. Each crossed majorant
// cell supplies a local upper density bound for tracking.
VolumeMajorantIterator MakeVolumeMajorantIterator( VolumeRecord medium, float3 origin, float3 direction,
    float entry, float exit ) {
    VolumeMajorantIterator state = ( VolumeMajorantIterator )0;
    state.mTime = entry;
    state.mEnd = exit;
    state.mGrid = medium.mFlags.y == 3 && medium.mMajorantGrid.w != 0;
    state.mRemaining = state.mGrid ? medium.mMajorantGrid.x + medium.mMajorantGrid.y + medium.mMajorantGrid.z + 3 : 1;
    if ( !state.mGrid ) {
        return state;
    }

    float3 o = mul( medium.mWorldToLocal, float4( origin, 1 ) ).xyz + .5;
    float3 d = mul( medium.mWorldToLocal, float4( direction, 0 ) ).xyz;
    float3 size = float3( medium.mMajorantGrid.xyz );
    state.mCell = clamp( int3( floor( ( o + d * entry ) * size ) ), int3( 0 ), int3( medium.mMajorantGrid.xyz ) - 1 );
    for ( uint axis = 0; axis < 3; ++axis ) {
        state.mAdvance[axis] = d[axis] >= 0 ? 1 : -1;
        if ( abs( d[axis] ) < 1e-30 ) {
            state.mCrossing[axis] = state.mDelta[axis] = 1e30;
        }
        else {
            float boundary = ( state.mCell[axis] + ( d[axis] > 0 ? 1 : 0 ) ) / size[axis];
            state.mCrossing[axis] = max( entry, ( boundary - o[axis] ) / d[axis] );
            state.mDelta[axis] = 1 / ( size[axis] * abs( d[axis] ) );
        }
    }
    return state;
}

// Return the next cell interval and advance the DDA to its nearest grid boundary. Non-grid
// media provide a single interval with their global bound.
bool NextVolumeMajorant( uint64_t data, VolumeRecord medium, inout VolumeMajorantIterator state,
    out float start, out float end, out float densityBound ) {
    start = state.mTime;
    end = state.mEnd;
    densityBound = 1;
    if ( state.mRemaining == 0 || start >= state.mEnd ) {
        return false;
    }
    --state.mRemaining;
    if ( !state.mGrid ) {
        state.mTime = end;
        return true;
    }

    uint index = uint( state.mCell.x ) + medium.mMajorantGrid.x *
                                            ( uint( state.mCell.y ) + medium.mMajorantGrid.y * uint( state.mCell.z ) );
    densityBound = LoadBufferValue<float>( data, medium.mMajorantGrid.w + index * sizeof( float ) );
    end = min( state.mEnd, min( state.mCrossing.x, min( state.mCrossing.y, state.mCrossing.z ) ) );
    state.mTime = end;
    if ( end >= state.mEnd ) {
        state.mRemaining = 0;
        return true;
    }
    for ( uint axis = 0; axis < 3; ++axis ) {
        if ( state.mCrossing[axis] <= end ) {
            state.mCell[axis] += state.mAdvance[axis];
            state.mCrossing[axis] += state.mDelta[axis];
        }
    }
    if ( any( state.mCell < 0 ) || any( state.mCell >= int3( medium.mMajorantGrid.xyz ) ) ) {
        state.mRemaining = 0;
    }
    return true;
}

// Retain progress when a dense medium's sampled flight is smaller than the
// floating-point spacing at a distant entry. Empty cells consume no RNG/work.
// Invert the exponential free-flight distribution for a majorant extinction. The sampled
// distance is measured along the world ray.
float VolumeFlightDistance( float start, float majorant, inout uint random ) {
    float distance = -log( max( 1 - Random( random ), 1e-7 ) ) / majorant;
    float result = start + distance;
    return result > start ? result : asfloat( asuint( start ) + 1u );
}

// Propose a null-collision candidate; RGB event weights are evaluated only
// after the scene has chosen the nearest candidate across overlapping media.
// Walk majorant intervals until an exponential free-flight sample lands inside one. Empty
// intervals advance without evaluating detailed density.
bool SampleVolumeCandidate( uint64_t data, VolumeRecord medium, float3 origin, float3 direction,
    float entry, float exit, inout uint random, out float distance, out float majorant ) {
    distance = exit;
    majorant = 0;
    float extinction = max( medium.mExtinctionMajorant.x, max( medium.mExtinctionMajorant.y, medium.mExtinctionMajorant.z ) );
    if ( extinction <= 0 || exit <= entry ) {
        return false;
    }
    if ( medium.mFlags.y <= 1 ) {
        float integral = -log( max( 1 - Random( random ), 1e-7 ) ) / extinction;
        float local;
        if ( !SampleVolumeAnalytic( MakeVolumeAnalyticProfile( medium, origin, direction, entry, exit ), integral, local ) ) {
            return false;
        }
        distance = entry + local;
        majorant = extinction;
        if ( medium.mFlags.y == 1 ) {
            majorant *= exp( clamp( -medium.mNoise.x * ( origin.y + direction.y * distance - medium.mDimensionsHeight.w ), -80.0, 40.0 ) );
        }
        return true;
    }
    VolumeMajorantIterator iterator = MakeVolumeMajorantIterator( medium, origin, direction, entry, exit );
    float start;
    float end;
    float bound;
    while ( NextVolumeMajorant( data, medium, iterator, start, end, bound ) ) {
        majorant = extinction * bound * 1.00001;
        if ( majorant <= 0 || end <= start ) {
            continue;
        }
        float candidate = VolumeFlightDistance( start, majorant, random );
        if ( candidate < end ) {
            distance = candidate;
            return true;
        }
    }
    return false;
}

// Estimate segment transmittance by ratio tracking inside the majorant intervals. Each
// candidate multiplies the residual transmission relative to its conservative bound.
float3 TrackVolumeTransmittance( uint64_t data, VolumeRecord medium, float3 origin, float3 direction,
    float entry, float exit, inout uint random ) {
    if ( medium.mFlags.y <= 1 ) {
        float integral = VolumeAnalyticIntegral( MakeVolumeAnalyticProfile( medium, origin, direction, entry, exit ) );
        return exp( -medium.mExtinctionMajorant.xyz * integral );
    }
    float3 transmittance = 1;
    float maximum = max( medium.mExtinctionMajorant.x, max( medium.mExtinctionMajorant.y, medium.mExtinctionMajorant.z ) );
    float3 coefficient = medium.mAbsorptionDensity.xyz + medium.mScatteringPhase.xyz;
    VolumeMajorantIterator iterator = MakeVolumeMajorantIterator( medium, origin, direction, entry, exit );
    float start;
    float end;
    float bound;
    while ( NextVolumeMajorant( data, medium, iterator, start, end, bound ) ) {
        float majorant = maximum * bound * 1.00001;
        if ( majorant <= 0 || end <= start ) {
            continue;
        }
        float t = start;
        while ( true ) {
            t = VolumeFlightDistance( t, majorant, random );
            if ( t >= end ) {
                break;
            }
            float density = VolumeDensity( data, medium, origin + direction * t, false );
            transmittance *= max( 0.0, 1 - coefficient * density / majorant );
            float survive = max( transmittance.x, max( transmittance.y, transmittance.z ) );
            if ( survive < .1 ) {
                if ( survive <= 0 || Random( random ) >= survive ) {
                    return 0;
                }
                transmittance /= survive;
            }
        }
    }
    return transmittance;
}
