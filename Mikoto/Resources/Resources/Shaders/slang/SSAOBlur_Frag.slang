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

// ============================================================================
// SSAO Blur Fragment Shader (Slang)
// ============================================================================

#include "base/Base.slang"

[[vk::binding(Slot_0, STATIC_SAMPLERS_SET_INDEX)]] Sampler2D u_samplerSSAO;

struct FSInput {
    float2 UV : TEXCOORD0;
};

struct FSOutput {
    float AO : SV_Target0;
};

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    int n = 0;
    const int blurRange = 2;

    // Texture size lookup
    uint width, height;
    u_samplerSSAO.GetDimensions(width, height);

    float2 texelSize = 1.0 / float2(width, height);

    float result = 0.0;
    for (int x = -blurRange; x <= blurRange; x++) {
        for (int y = -blurRange; y <= blurRange; y++) {
            float2 offset = float2(x, y) * texelSize;
            result += u_samplerSSAO.Sample(input.UV + offset).r;
            n++;
        }
    }

    o.AO = result / float(n);
    return o;
}