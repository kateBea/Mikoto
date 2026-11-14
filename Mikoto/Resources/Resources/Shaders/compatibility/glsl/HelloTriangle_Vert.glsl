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

#version 430 core

// GLSL counterpart of the interpolated-color sample; uses OpenGL's gl_VertexID.
// https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/shaders.hlsl
// Draw three vertices as a triangle list, starting at vertex 0.
layout( location = 0 ) out vec3 mColor;

const vec2 kPositions[3] = vec2[](
    vec2( -0.6, -0.5 ),
    vec2( 0.6, -0.5 ),
    vec2( 0.0, 0.6 )
);

const vec3 kColors[3] = vec3[](
    vec3( 1.0, 0.0, 0.0 ),
    vec3( 0.0, 1.0, 0.0 ),
    vec3( 0.0, 0.0, 1.0 )
);

void main() {
    gl_Position = vec4( kPositions[gl_VertexID], 0.0, 1.0 );
    mColor = kColors[gl_VertexID];
}
