//    Copyright 2025 ケイト
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

// Reference types
// https://docs.shader-slang.org/en/latest/coming-from-glsl.html#option-2-glsl-style-layout-syntax

// For the sake of not having to handle this
// geometry in CPU side I define it here

static const float3 boxPositions[36] = float3[](
    float3(-1.0,  1.0, -1.0),
    float3(-1.0, -1.0, -1.0),
    float3( 1.0, -1.0, -1.0),
    float3( 1.0, -1.0, -1.0),
    float3( 1.0,  1.0, -1.0),
    float3(-1.0,  1.0, -1.0),

    float3(-1.0, -1.0,  1.0),
    float3(-1.0, -1.0, -1.0),
    float3(-1.0,  1.0, -1.0),
    float3(-1.0,  1.0, -1.0),
    float3(-1.0,  1.0,  1.0),
    float3(-1.0, -1.0,  1.0),

    float3( 1.0, -1.0, -1.0),
    float3( 1.0, -1.0,  1.0),
    float3( 1.0,  1.0,  1.0),
    float3( 1.0,  1.0,  1.0),
    float3( 1.0,  1.0, -1.0),
    float3( 1.0, -1.0, -1.0),

    float3(-1.0, -1.0,  1.0),
    float3(-1.0,  1.0,  1.0),
    float3( 1.0,  1.0,  1.0),
    float3( 1.0,  1.0,  1.0),
    float3( 1.0, -1.0,  1.0),
    float3(-1.0, -1.0,  1.0),

    float3(-1.0,  1.0, -1.0),
    float3( 1.0,  1.0, -1.0),
    float3( 1.0,  1.0,  1.0),
    float3( 1.0,  1.0,  1.0),
    float3(-1.0,  1.0,  1.0),
    float3(-1.0,  1.0, -1.0),

    float3(-1.0, -1.0, -1.0),
    float3(-1.0, -1.0,  1.0),
    float3( 1.0, -1.0, -1.0),
    float3( 1.0, -1.0, -1.0),
    float3(-1.0, -1.0,  1.0),
    float3( 1.0, -1.0,  1.0)
);
