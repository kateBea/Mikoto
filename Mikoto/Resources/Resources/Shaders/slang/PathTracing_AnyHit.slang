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

#include "PathTracing_Common.slang"

// Standalone any-hit example. The production pipeline evaluates material
// coverage in raygen so the same acceptance rules serve transport and shadows.
// https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#any-hit-shaders
[shader( "anyhit" )]
void main( inout Hit payload, BuiltInTriangleIntersectionAttributes attributes ) {
    static const float splitBarycentric = 0.5;

    if ( attributes.barycentrics.x < splitBarycentric ) {
        IgnoreHit();
    }
}