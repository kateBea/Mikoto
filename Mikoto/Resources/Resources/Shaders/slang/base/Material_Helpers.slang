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

#pragma once

struct MaterialParameters {
    float4 BaseColorFactor;
    float4 EmissiveFactor;
    float4 DiffuseFactor;
    float4 SpecularFactor;

    int    Workflow;

    float  MetallicFactor;
    float  RoughnessFactor;
    float  AlphaMask;
    float  AlphaMaskCutoff;
    float  EmissiveStrength;

    int    BaseColorTextureSet;
    int    PhysicalDescriptorTextureSet;
    int    NormalTextureSet;
    int    OcclusionTextureSet;
    int    EmissiveTextureSet;

    int    NormalIndex;
    int    MetallicIndex;
    int    RoughnessIndex;
    int    AoIndex;
    int    EmissiveIndex;
};

struct MeshParameters {
    float4x4 Transform;
    float4x4 InverseModelView;
};