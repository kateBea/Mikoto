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

// Build terrain-relative water depth, shoreline and foam controls. Sampling the coast field
// avoids repeating terrain queries in every water-surface fragment.

#include "base/Water_Helpers.slang"

struct CoastParameters {
    Pointer mSurfaces;
    Pointer mWaves;
    Pointer mDisplacement;
    Pointer mGradients;
    uint mSurface;
};
MKT_DECLARE_PUSH_CONSTANT( CoastParameters, parameters );

[shader( "compute" )]
[numthreads( kImageGroupWidth, kImageGroupHeight, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    WaterSurface surface = LoadBufferValue<WaterSurface>( parameters.mSurfaces, parameters.mSurface * sizeof( WaterSurface ) );
    uint side = surface.mCoastRange.y;
    if ( any( thread.xy >= side ) ) {
        return;
    }
    float2 rest = surface.mCoastPatch.xy + ( float2( thread.xy ) + .5 ) * surface.mCoastPatch.zw;
    float bed = WaterBedDepth( surface, rest );
    float weight = smoothstep( 0, max( surface.mCoastSettings.y, .1 ), bed );
    float3 displacement = 0;
    float3 tx = float3( 1, 0, 0 );
    float3 tz = float3( 0, 0, 1 );
    if ( surface.mSimulation.z == kWaterSimulationFft ) {
        WaterFftSample sample = GetWaterFftSample( rest, surface.mFft.x );
        displacement = SampleWaterFft( parameters.mDisplacement, surface.mSimulation.w, sample, 1 ).xyz;
        tx = SampleWaterFft( parameters.mGradients, surface.mSimulation.w * 2, sample, 2 ).xyz;
        tz = SampleWaterFft( parameters.mGradients, surface.mSimulation.w * 2 + 1, sample, 2 ).xyz;
    }
    else {
        float3 position;
        EvaluateWater( float3( rest.x, 0, rest.y ), surface, parameters.mWaves, position, tx, tz );
        displacement = position - float3( rest.x, 0, rest.y );
    }
    tx = float3( 1, 0, 0 ) + ( tx - float3( 1, 0, 0 ) ) * weight;
    tz = float3( 0, 0, 1 ) + ( tz - float3( 0, 0, 1 ) ) * weight;
    float compression = 1 - smoothstep( .55, .88, tx.x * tz.z - tx.z * tz.x );
    float verticalScale = length( mul( ( float3x3 )surface.mWorld, float3( 0, 1, 0 ) ) );
    float wetDepth = bed + displacement.y * weight * verticalScale;
    float shore = ( surface.mGeometry.w & 4 ) != 0 ? saturate( 1 - max( wetDepth, 0.0 ) / surface.mCoast.x ) * surface.mCoast.y : 0;
    float source = max( compression * surface.mOptics.w, shore );
    if ( wetDepth < -.05 || ( ( surface.mGeometry.w & 64u ) == 0 && ( any( rest < 0 ) || any( rest > surface.mBounds.xy ) ) ) ) {
        source = 0;
    }
    float lifetime = max( surface.mCoastSettings.x, .1 );
    float2 advected = rest - surface.mCoastSettings.zw * surface.mCoast.z * surface.mCoastPrevious.z * surface.mCoastPatch.zw;
    float2 previousGrid = ( advected - surface.mCoastPrevious.xy ) / surface.mCoastPatch.zw;
    bool retained = surface.mCoastPrevious.w == 0 && all( previousGrid >= .5 ) && all( previousGrid <= side - 1.5 );
    // Exact exponential integration avoids frame-rate-dependent accumulation.
    float decay = exp( -surface.mCoastPrevious.z / lifetime );
    float old = retained ? WaterCoastLookup( surface.mCoastHistory, surface, advected, surface.mCoastPrevious.xy ).y : source;
    float foam = lerp( source, old, decay );
    if ( !retained ) {
        foam = source;// Prewarm newly exposed cells to equilibrium.
    }
    RWBufferReference<float4> output = ( RWBufferReference<float4> )surface.mCoastField;
    output[surface.mCoastRange.x + thread.y * side + thread.x] = float4( bed, saturate( foam ), weight, 0 );
}
