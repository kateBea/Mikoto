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

// Match CompatibilityFrameModule's frame uniform.
static const uint kMaxLights = 32;

struct Light {
    float4 PositionType;
    float4 DirectionRadius;
    float4 Radiance;
    float4 Spot;
};

struct FrameParameters {
    float4x4 ViewProjection;
    float4x4 InverseSkyViewProjection;
    float4 CameraPosition;
    float4 Environment; // strength, sky blur, last mip, source type
    uint4 Lighting; // light count and one-based shadow light index
    Light Lights[kMaxLights];
    float4x4 ShadowViewProjection;
};

[[vk::binding( 0, 0 )]] ConstantBuffer<FrameParameters> frame : register( b0 );
