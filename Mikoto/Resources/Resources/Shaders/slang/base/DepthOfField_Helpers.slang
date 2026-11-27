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

// Depth-of-field parameters, signed CoC conversion and aperture sampling. Gather buffers use
// negative CoC for foreground and positive CoC for background layers.

#pragma once

#include "Base.slang"
#include "Memory_Helpers.slang"

// Matches the 112-byte scalar layout in DisplayEffectsModule.cc.
// Tile layout and slots mirror DisplayEffectsModule; the radiance limit is the largest finite binary16.
static const uint kDoFTileSize = 8;
static const uint kDoFTileLaneCount = 64;
static const uint kDoFApertureAreaSamples = 64;
static const uint kDoFMedianSampleCount = 9;
static const int kDoFMedianCenter = 4;
static const int kDoFApertureInput = 5;
static const int kDoFTileBoundsInput = 4;
static const float kDoFMaxHalfFloat = 65504.0;

// Matches the fixed texture-slot arrays in the CPU DepthOfFieldParameters record.
static const uint kDoFInputSlotCount = 8;
static const uint kDoFOutputSlotCount = 2;

struct DepthOfFieldParameters {
    uint mInputs[kDoFInputSlotCount];
    uint mOutputs[kDoFOutputSlotCount];
    uint mSampler;
    uint mWidth;
    uint mHeight;
    uint mTargetWidth;
    uint mTargetHeight;
    uint mRingCount;
    float mCoCScale;
    float mFocusDistance;
    float mMaxRadius;
    float mBladeCount;
    float mBladeCurvature;
    float mBladeRotation;
    float mNearPlane;
    float mFarPlane;
    uint mMipLevel;
    uint mEnabled;
    uint mVisualize;
    uint mPadding;
};

MKT_DECLARE_TEXTURE_SRV( Texture2D<float4> ) dofImages;
// Slang storage format 2 is rgba16f, matching every DoF UAV. Putting the format
// in the texture type also preserves it through D3D12's ResourceArray wrapper.
typedef RWTexture2D<float4, 0, 2> DepthOfFieldStorageImage;
MKT_DECLARE_TEXTURE_UAV( DepthOfFieldStorageImage ) dofOutputs;
MKT_DECLARE_SAMPLERS dofSamplers;

static const float kDoFPi = 3.141592653589793;

uint2 DoFHalfSize( DepthOfFieldParameters p ) {
    return uint2( ( p.mWidth + 1 ) / 2, ( p.mHeight + 1 ) / 2 );
}

float4 DoFLoad( uint image, int2 pixel ) {
    uint width;
    uint height;
    dofImages[image].GetDimensions( width, height );
    return dofImages[image].Load( int3( clamp( pixel, int2( 0 ), int2( width, height ) - 1 ), 0 ) );
}

float4 DoFSample( uint image, uint samplerID, float2 uv ) {
    return dofImages[image].SampleLevel( dofSamplers[samplerID], saturate( uv ), 0 );
}

float3 DoFClampColor( float3 color ) {
    return clamp( color, 0.0, kDoFMaxHalfFloat );
}

float DoFLuminance( float3 color ) {
    return dot( max( color, 0.0 ), float3( 0.2126, 0.7152, 0.0722 ) );
}

// Convert view depth and lens parameters into a signed blur radius in pixels. Its sign selects
// foreground/background handling and its magnitude selects the gather footprint.
float DoFCoC( float depth, DepthOfFieldParameters p ) {
    // Perspective RH_ZO depth: 0 is the near plane, 1 is the far plane.
    float distance = p.mNearPlane * p.mFarPlane /
        max( p.mFarPlane - saturate( depth ) * ( p.mFarPlane - p.mNearPlane ), 1e-6 );
    return clamp( p.mCoCScale * ( 1.0 - p.mFocusDistance / max( distance, 1e-6 ) ),
                  -p.mMaxRadius, p.mMaxRadius );
}

// Find the aperture boundary at this polar angle. Blade count and roundness change the bokeh
// silhouette while retaining the shared radius convention.
float DoFApertureRadius( float angle, DepthOfFieldParameters p ) {
    float sector = 2.0 * kDoFPi / max( p.mBladeCount, 3.0 );
    float localAngle = angle - p.mBladeRotation;
    localAngle -= sector * floor( ( localAngle + 0.5 * sector ) / sector );
    float polygon = cos( 0.5 * sector ) / max( cos( localAngle ), 1e-4 );
    return lerp( polygon, 1.0, saturate( p.mBladeCurvature ) );
}

// Estimate aperture area by angular integration. Coverage normalization must use the same
// aperture shape as sample placement.
float DoFApertureArea( DepthOfFieldParameters p ) {
    float sum = 0.0;
    for ( uint i = 0; i < kDoFApertureAreaSamples; ++i ) {
        float radius = DoFApertureRadius( ( i + 0.5 ) * ( 2.0 * kDoFPi / float( kDoFApertureAreaSamples ) ), p );
        sum += radius * radius;
    }
    return sum / float( kDoFApertureAreaSamples ); // Fraction of the enclosing disk's area.
}

float2 DoFApertureSample( float2 disk, DepthOfFieldParameters p ) {
    return DoFSample( p.mInputs[kDoFApertureInput], p.mSampler, disk * 0.5 + 0.5 ).xy;
}

// Choose a gather source level from blur footprint and CoC variation. Strong near/far variation
// limits how much detail may be reduced.
uint DoFGatherMip( float radius, float4 bounds, DepthOfFieldParameters p ) {
    // Coarse bilateral mips are safe only when the layer's CoC is nearly uniform.
    bool foreground = bounds.x < -0.25;
    bool background = bounds.w > 0.25;
    float variation = max( abs( bounds.y - bounds.x ), abs( bounds.w - bounds.z ) );
    if ( ( foreground && background ) || variation > max( 0.5, radius * 0.05 ) ) {
        return 0;
    }
    float spacing = radius / max( float( p.mRingCount ) + 0.5, 1.0 );
    return uint( clamp( floor( log2( max( spacing, 1.0 ) ) ), 0.0, 3.0 ) );
}

float4 DoFLoadMip( float2 halfPixel, uint mip, DepthOfFieldParameters p ) {
    return DoFLoad( p.mInputs[mip], int2( floor( ( halfPixel + 0.5 ) / float( 1u << mip ) ) ) );
}

// Take the componentwise median of nine gathered values. Sorting the small neighborhood
// suppresses isolated bright/coverage samples without another wide blur.
float4 DoFMedian9( float4 values[kDoFMedianSampleCount] ) {
    // Sort each RGBA component independently; the middle value rejects isolated gather noise.
    for ( uint i = 1; i < kDoFMedianSampleCount; ++i ) {
        for ( uint j = i; j > 0; --j ) {
            float4 low = min( values[j - 1], values[j] );
            float4 high = max( values[j - 1], values[j] );
            values[j - 1] = low;
            values[j] = high;
        }
    }
    return values[kDoFMedianCenter];
}
