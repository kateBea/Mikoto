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

// Test uploaded draw bounds against the camera frustum and update indirect draw visibility.
// Draw slots remain stable while invisible commands are disabled for later raster passes.

#pragma once

#include "base/Base.slang"

// The CPU uploads six frustum planes and dispatches one thread per draw in groups of 64.
static const uint kCullFrustumPlaneCount = 6;
static const uint kCullGroupSize = 64;

struct DrawCommand {
    uint mVertexCount;
    uint mInstanceCount;
    uint mFirstVertex;
    uint mFirstInstance;
};

struct DrawBounds {
    float4 mCenter;
    float4 mExtent;
};

struct CullParameters {
    float4 mPlanes[kCullFrustumPlaneCount];

    BufferReference<DrawCommand> mSource;
    BufferReference<DrawBounds> mBounds;
    RWBufferReference<DrawCommand> mOutput;

    uint mCount;
    uint mEnabled;
};
MKT_DECLARE_PUSH_CONSTANT( CullParameters, parameters );

[shader( "compute" )]
[numthreads( kCullGroupSize, 1, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    uint index = thread.x;
    if ( index >= parameters.mCount ) {
        return;
    }

    DrawCommand command = parameters.mSource[index];

    // Copy the original command even when testing is disabled. Reusing the last
    // culled output would leave previously hidden draws with zero instances.
    if ( parameters.mEnabled == 0 ) {
        parameters.mOutput[index] = command;
        return;
    }

    DrawBounds bounds = parameters.mBounds[index];

    // Animated/invalid bounds are conservative: retain the draw. A hidden draw
    // gets zero instances without changing IDs, batch ranges or blend ordering.
    if ( bounds.mExtent.w > 0 ) {
        for ( uint i = 0; i < kCullFrustumPlaneCount; ++i ) {
            float4 plane = parameters.mPlanes[i];
            float distance = dot( plane.xyz, bounds.mCenter.xyz ) + plane.w;
            float radius = dot( abs( plane.xyz ), bounds.mExtent.xyz );
            float tolerance =
                    1e-5 * ( dot( abs( plane.xyz ), abs( bounds.mCenter.xyz ) ) + abs( plane.w ) + radius + 1 );
            if ( distance + radius < -tolerance ) {
                command.mInstanceCount = 0;
                break;
            }
        }
    }
    parameters.mOutput[index] = command;
}
