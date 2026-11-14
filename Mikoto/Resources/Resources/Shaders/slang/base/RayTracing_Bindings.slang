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

#include "Memory_Helpers.slang"

// The frame graph binds the shared table at the default set. The standalone
// showcase moves the table to set 1 because its private resources occupy set 0.
// https://docs.vulkan.org/spec/latest/chapters/descriptorsets.html
// https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_DynamicResources.html
#if !defined( MKT_RAY_SCENE_SET )
#define MKT_RAY_SCENE_SET MKT_DEFAULT_REGISTER_SPACE
#endif

#if !defined( MKT_SUPPORT_D3D12 )
MKT_DECLARE_ACCEL_STRUCTURES_IN_SET( MKT_RAY_SCENE_SET ) RayScenes;
#endif

RaytracingAccelerationStructure LoadRayScene( uint index ) {
#if defined( MKT_SUPPORT_D3D12 )
    // A uniform heap index keeps TraceRay's scene selection uniform.
    return getDescriptorFromHandle( DescriptorHandle<RaytracingAccelerationStructure>( uint2( index, 0 ) ) );
#else
    return RayScenes[index];
#endif
}
