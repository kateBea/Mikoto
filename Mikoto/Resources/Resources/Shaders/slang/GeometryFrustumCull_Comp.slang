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

#include "base/Base.slang"

struct DrawCommand {
    uint vertexCount;
    uint instanceCount;
    uint firstVertex;
    uint firstInstance;
};

struct DrawBounds {
    float4 center;
    float4 extent;
};

struct CullParameters {
    float4 planes[6];

    BufferReference<DrawCommand> source;
    BufferReference<DrawBounds> bounds;
    RWBufferReference<DrawCommand> output;

    uint count;
    uint enabled;
};
MKT_DECLARE_PUSH_CONSTANT( CullParameters, parameters );

[shader( "compute" )]
[numthreads( 64, 1, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    uint index = thread.x;
    if ( index >= parameters.count ) {
        return;
    }

    DrawCommand command = parameters.source[index];

    // Copy the original command even when testing is disabled. Reusing the last
    // culled output would leave previously hidden draws with zero instances.
    if ( parameters.enabled == 0 ) {
        parameters.output[index] = command;
        return;
    }

    DrawBounds bounds = parameters.bounds[index];

    // Animated/invalid bounds are conservative: retain the draw. A hidden draw
    // gets zero instances without changing IDs, batch ranges or blend ordering.
    if ( bounds.extent.w > 0 ) {
        for ( uint i = 0; i < 6; ++i ) {
            float4 plane = parameters.planes[i];
            float distance = dot( plane.xyz, bounds.center.xyz ) + plane.w;
            float radius = dot( abs( plane.xyz ), bounds.extent.xyz );
            float tolerance =
                    1e-5 * ( dot( abs( plane.xyz ), abs( bounds.center.xyz ) ) + abs( plane.w ) + radius + 1 );
            if ( distance + radius < -tolerance ) {
                command.instanceCount = 0;
                break;
            }
        }
    }
    parameters.output[index] = command;
}
