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

// Resolve small blur radii with a compact full-neighborhood gather. This handles the transition
// between sharp pixels and the wider half-resolution bokeh layers.

#include "base/DepthOfField_Helpers.slang"

// Maximum full-resolution gather radius in pixels; wider blur is resolved by the half-
// resolution bokeh layers.
static const int kDoFSmallGatherRadius = 3;

MKT_DECLARE_PUSH_CONSTANT( DepthOfFieldParameters, parameters );

[shader( "compute" )]
[numthreads( kImageGroupWidth, kImageGroupHeight, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    if ( any( thread.xy >= uint2( parameters.mTargetWidth, parameters.mTargetHeight ) ) ) { return; }
    float4 center = DoFLoad( parameters.mInputs[0], int2( thread.xy ) );
    float4 bounds = DoFLoad( parameters.mInputs[1], int2( thread.xy / kDoFTileSize ) );
    if ( bounds.x < -3.0 && bounds.y < -3.0 && bounds.w <= 0.0 ) {
        dofOutputs[parameters.mOutputs[0]][thread.xy] = float4( center.rgb, 0 );
        return;
    }

    float3 color = 0.0;
    float sum = 0.0;
    float opacity = 0.0;
    // Deliberately brute force: this buffer bounds the more accurate full-resolution
    // gather instead of spatially upscaling a coarse blur into slightly defocused detail.
    for ( int y = -kDoFSmallGatherRadius; y <= kDoFSmallGatherRadius; ++y ) {
        for ( int x = -kDoFSmallGatherRadius; x <= kDoFSmallGatherRadius; ++x ) {
            float2 offset = float2( x, y );
            if ( dot( offset, offset ) > 9.0 ) { continue; }
            float4 shape = DoFSample( parameters.mInputs[2], parameters.mSampler, offset / 6.0 + 0.5 );
            float4 sample = DoFLoad( parameters.mInputs[0], int2( thread.xy ) + int2( x, y ) );
            float radius = abs( sample.a );
            if ( radius > 3.0 ) { continue; }
            float distance = length( offset ) * shape.x;
            float intersection = saturate( max( radius, 0.5 ) - distance + 0.5 );
            float depthWeight = saturate( 1.0 - max( sample.a - center.a, 0.0 ) / max( abs( center.a ), 1.0 ) );
            float weight = intersection * depthWeight / max( radius * radius * shape.y * kDoFPi, 1.0 );
            color += sample.rgb * weight;
            sum += weight;
            opacity += intersection * depthWeight / max( radius * radius * shape.y * kDoFPi, 1.0 );
        }
    }
    float3 result = sum > 1e-5 ? color / sum : center.rgb;
    dofOutputs[parameters.mOutputs[0]][thread.xy] = float4( DoFClampColor( result ), saturate( opacity ) );
}
