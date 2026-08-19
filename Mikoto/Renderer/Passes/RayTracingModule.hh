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

#ifndef MIKOTO_RAY_TRACING_HH
#define MIKOTO_RAY_TRACING_HH

// Refs:
// https://juanraul8.github.io/PBR-vulkan/
// https://developer.nvidia.com/rtx/raytracing/vkray
// https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR
// https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/
// https://docs.vulkan.org/samples/latest/samples/extensions/ray_tracing_basic/README.html
// https://github.com/KhronosGroup/Vulkan-Samples/tree/main/samples/extensions/ray_tracing_basic
// https://www.saschawillems.de/blog/2019/04/27/vulkan-examples-for-ray-traced-shadows-and-reflections-using-vk_nv_ray_tracing/

// https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Advanced_Topics/Planar_Reflections.html

// https://www.khronos.org/blog/ray-tracing-in-vulkan
// https://docs.vulkan.org/tutorial/latest/courses/18_Ray_tracing/00_Overview.html

// https://developer.nvidia.com/rtx/raytracing/dxr/dx12-raytracing-tutorial-part-1
// https://developer.nvidia.com/rtx/raytracing/dxr/DX12-Raytracing-tutorial-Part-2

// https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html

#include <Renderer/Core/FrameGraph.hh>

namespace mikoto::renderer {

    class RayTracingModule final {
    public:
        auto RegisterPasses( FrameGraph& graph ) -> void;

    private:

    };
}

#endif//MIKOTO_RAY_TRACING_HH
