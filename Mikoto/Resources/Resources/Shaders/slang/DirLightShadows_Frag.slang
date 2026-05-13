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
#include "base/Camera_Helpers.slang"
#include "base/Lighting_Helpers.slang"

struct FSOutput {
    float4 Color : SV_Target0;
};

struct FSInput {
    float4 ScreenPos : SV_Position;
};

[[vk::binding(Slot_0, BUFFER_VIEWS_SET_INDEX)]]
ConstantBuffer<CameraInfo, ScalarDataLayout> u_Camera;

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    float depth = LinearDepth(input.ScreenPos.z, u_Camera.Planes);

    float near = u_Camera.Planes.x;
    float far = u_Camera.Planes.y;

    float depthNormalized = (depth - near) / (far - near);

    o.Color = float4(depthNormalized, depthNormalized, depthNormalized, 1.0);

    return o;
}