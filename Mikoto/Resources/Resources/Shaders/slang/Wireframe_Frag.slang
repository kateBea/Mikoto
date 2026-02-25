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

#include "base/Base.slang"

struct FSOutput {
    float4 Color : SV_Target0;
};

struct WireframeParams {
    float4 LineColor;
};

[[vk::push_constant]]
WireframeParams u_Parameters;

[shader("fragment")]
FSOutput main() {
    FSOutput o;

    o.Color = u_Parameters.LineColor;

    return o;
}