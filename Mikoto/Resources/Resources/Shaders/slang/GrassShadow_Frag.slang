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

// Apply grass placement masks and stable world-space dithering to the canopy proxy. Sampled
// blocker heights approximate a tapered canopy without drawing every blade into every cascade.

#include "base/GrassShadow_Helpers.slang"

static const float kGrassProxyMaximumCoverage = .98;
static const float kGrassProxyMinimumLightCosine = .12;

// Stable world-cell noise approximates canopy gaps and distributes blocker
// depths through the canopy. Shadow-map filtering turns these samples into
// partial visibility. No frame randomization is used, so shadows do not sparkle.
[shader( "fragment" )]
float main( GrassShadowVaryings input ) : SV_Depth {
    GrassDescription source = LoadBufferValue<GrassDescription>( parameters.mDescriptions, parameters.mDescription * sizeof( GrassDescription ) );
    clip( source.mShadows.x - .5 );
    float3 normal = normalize( cross( ddy( input.mBase ), ddx( input.mBase ) ) );
    if ( abs( normal.y ) < source.mLimits.w || input.mBase.y < source.mLimits.x || input.mBase.y > source.mLimits.y ) {
        discard;
    }

    // Reapply terrain paint weights, slope/elevation limits and palette selection. The proxy
    // must cast only where the corresponding grass variant can actually grow.
    TerrainSurfaceData surface = LoadBufferValue<TerrainSurfaceData>( source.mSurface, 0 );
    float weight = TerrainWeights( surface, input.mGridHeight.xy, input.mGridHeight.z, abs( normal.y ) )[source.mControl.x];
    uint seed = GrassCellHash( int2( floor( input.mLocal.xz / source.mTiling.x ) ), source.mRanges.w );
    float choice = GrassRandom( seed + kGrassProxyPaletteSalt ) * source.mPalette.z;
    if ( choice < source.mPalette.x || choice >= source.mPalette.y ) {
        discard;
    }

    // Match the visible field's distance thinning and density budget, including
    // width compensation. Off-screen patches still cast while inside the field.
    CameraInfo camera = LoadBufferValue<CameraInfo>( parameters.mCamera, 0 );
    float distance = length( input.mBase - camera.mViewPosition.xyz );
    float ratio = source.mDetail.y / max( distance, source.mDetail.y );
    float density = ratio * ratio * lerp( 1, source.mDetail.x,
        smoothstep( source.mView.y, source.mView.x, distance ) ) * source.mBudgets.x;
    density *= 1 - smoothstep( source.mView.x * .85, source.mView.x, distance );
    float3 xAxis = mul( source.mWorld, float4( 1, 0, 0, 0 ) ).xyz;
    float3 zAxis = mul( source.mWorld, float4( 0, 0, 1, 0 ) ).xyz;
    float scale = length( mul( source.mWorld, float4( 0, 1, 0, 0 ) ).xyz );
    float roots = density / max( source.mTiling.x * source.mTiling.x * length( cross( xAxis, zAxis ) ), .0001 );
    float compensation = lerp( 1, min( 4, rcp( max( density, .01 ) ) ), source.mLighting.w );
    float height = length( input.mCanopy - input.mBase );
    float3 toLight = normalize( parameters.mToLight );
    normal *= normal.y < 0 ? -1 : 1;
    float signedLightCosine = dot( normal, toLight );
    float lightCosine = abs( signedLightCosine );
    float projected = kGrassProxyVerticalCoverage + sqrt( saturate( 1 - lightCosine * lightCosine ) );

    // Estimate projected blade coverage from roots per area, blade width/height and light
    // angle. Beer-Lambert coverage turns this estimate into the probability of retaining a
    // proxy fragment.
    float opticalDepth = roots * source.mShape.y * scale * compensation * height * projected *
        source.mShadows.y / max( lightCosine, kGrassProxyMinimumLightCosine );
    float coverage = min( kGrassProxyMaximumCoverage, ( 1 - exp( -opticalDepth ) ) * weight );
    if ( GrassRandom( seed + kGrassProxyCoverageSalt ) >= coverage ) {
        discard;
    }

    // Tapered blades place more blockers near their roots. Sample depth along
    // the LIGHT RAY so shadow-map XY stays fixed. Moving vertically and writing
    // only SV_Depth stores a different ray's depth, especially at grazing sun.
    // The sign also handles a terrain/canopy slab viewed from its underside.
    float blockerHeight = 1 - sqrt( 1 - GrassRandom( seed + kGrassProxyDepthSalt ) );
    float thickness = dot( input.mCanopy - input.mBase, normal );
    float rayDistance = thickness * ( 1 - blockerHeight ) /
        max( lightCosine, kGrassProxyMinimumLightCosine );
    rayDistance *= signedLightCosine < 0 ? -1 : 1;
    float3 blocker = input.mCanopy - toLight * rayDistance;
    float4x4 matrix = LoadBufferValue<float4x4>( parameters.mRecords,
        parameters.mRecord * sizeof( ShadowRecord ) + parameters.mFace * sizeof( float4x4 ) );
    float4 clipPosition = mul( matrix, float4( blocker, 1 ) );
    return saturate( clipPosition.z / clipPosition.w );
}

