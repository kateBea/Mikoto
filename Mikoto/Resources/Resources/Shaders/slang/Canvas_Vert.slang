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


#include "base/Canvas_Helpers.slang"

[shader( "vertex" )]
CanvasVaryings main( uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID ) {
    static const uint corners[6] = { 0, 1, 2, 0, 2, 3 };
    static const float2 coordinates[4] = { float2( 0, 0 ), float2( 1, 0 ), float2( 1, 1 ), float2( 0, 1 ) };

    uint index = parameters.offset + instanceID;

    CanvasQuad quad = ( ( BufferReference<CanvasQuad> )parameters.quads )[index];

    uint corner = corners[vertexID];

    CanvasVaryings output;

    output.index = index;
    output.position = quad.positions[corner];
    output.local = coordinates[corner];
    output.uv = lerp( quad.uv.xy, quad.uv.zw, output.local );
    
    return output;
}
