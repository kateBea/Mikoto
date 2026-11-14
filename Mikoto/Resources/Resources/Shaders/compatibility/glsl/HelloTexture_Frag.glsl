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

// GLSL counterpart of the texture sample, using a combined sampler:
// https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTexture/shaders.hlsl
// Host contract: bind both the texture and sampler object to texture unit 0.
layout( binding = 0 ) uniform sampler2D uTexture;

layout( location = 0 ) in vec2 mTexCoord;
layout( location = 0 ) out vec4 outColor;

void main() {
    outColor = texture( uTexture, mTexCoord );
}
