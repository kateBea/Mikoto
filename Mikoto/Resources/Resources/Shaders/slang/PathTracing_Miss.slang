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

// Mark a ray as having missed scene geometry. Environment radiance is evaluated by the calling
// integrator using the miss result.

#include "PathTracing_Common.slang"

[shader( "miss" )]
void main( inout Hit payload ) {
    // Li() handles escaped-ray emission and its MIS weight using the previous vertex.
    // https://pbr-book.org/4ed/Light_Transport_I_Surface_Reflection/A_Better_Path_Tracer
    payload.mDistance = RAY_MISS_DISTANCE;
}
