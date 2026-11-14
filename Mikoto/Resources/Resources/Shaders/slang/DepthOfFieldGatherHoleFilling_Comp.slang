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

#include "base/DepthOfField_Helpers.slang"

MKT_DECLARE_PUSH_CONSTANT( DepthOfFieldParameters, parameters );

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    if ( any( thread.xy >= uint2( parameters.mTargetWidth, parameters.mTargetHeight ) ) ) { return; }
    float coverage = DoFLoad( parameters.mInputs[6], int2( thread.xy ) ).a;
    float4 bounds = DoFLoad( parameters.mInputs[4], int2( thread.xy / 8 ) );
    if ( coverage <= 0.001 || coverage >= 0.999 || bounds.x >= -0.25 ) {
        dofOutputs[parameters.mOutputs[0]][thread.xy] = 0.0;
        return;
    }

    float4 center = DoFLoad( parameters.mInputs[0], int2( thread.xy ) );
    float radius = max( -bounds.x, 1.0 );
    uint rings = max( parameters.mRingCount, 1u );
    float3 color = 0.0;
    float sum = 0.0;
    float available = 0.0;
    float total = 0.0;
    for ( uint ring = 0; ring < rings; ++ring ) {
        uint count = 6 * ( ring + 1 );
        float radial = ( float( ring ) + 0.5 ) / float( rings );
        for ( uint i = 0; i < count; ++i ) {
            float angle = 2.0 * kDoFPi * ( i + 0.5 ) / float( count );
            float2 offset = DoFApertureSample( float2( cos( angle ), sin( angle ) ) * radial, parameters ) * radius;
            float4 sample = DoFLoad( parameters.mInputs[0], int2( round( float2( thread.xy ) + offset ) ) );
            // Prefer visible surfaces farther away than the occluding foreground,
            // including in-focus pixels; do not pretend to recover actually hidden geometry.
            float farther = saturate( ( sample.a - min( center.a, -0.5 ) ) * 2.0 );
            float spatial = 1.0 / ( 1.0 + dot( offset, offset ) );
            float weight = farther * spatial;
            color += sample.rgb * weight;
            sum += weight;
            available += farther;
            total += 1.0;
        }
    }
    float opacity = saturate( available / max( total, 1.0 ) * 2.0 );
    // Unpremultiplied color lets Recombine adjust fill opacity independently from foreground coverage.
    dofOutputs[parameters.mOutputs[0]][thread.xy] = float4( DoFClampColor( color / max( sum, 1e-5 ) ), sum > 1e-5 ? opacity : 0.0 );
}
