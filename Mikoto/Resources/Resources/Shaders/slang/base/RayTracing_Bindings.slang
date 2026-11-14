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

// Matches MKT_ACCELERATION_STRUCTURE_BINDING in ResourceBindings.hh.
// The production renderer uses the frame graph's table at set zero. The
// standalone showcase overrides this because its geometry occupies set zero.
#if !defined( MKT_RAY_SCENE_SET )
#define MKT_RAY_SCENE_SET 0
#endif
// https://docs.vulkan.org/spec/latest/chapters/descriptorsets.html
// https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_DynamicResources.html
#if !defined( MKT_SUPPORT_D3D12 )
[[vk::binding( 6, MKT_RAY_SCENE_SET )]] RaytracingAccelerationStructure RayScenes[];
#endif

RaytracingAccelerationStructure LoadRayScene( uint index ) {
#if defined( MKT_SUPPORT_D3D12 )
    return getDescriptorFromHandle( DescriptorHandle<RaytracingAccelerationStructure>( uint2( index, 0 ) ) );
#else
    // The push-constant index is uniform for the entire dispatch.
    return RayScenes[index];
#endif
}
