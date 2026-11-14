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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_PATH_TRACING_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_PATH_TRACING_MODULE_HH

#include <EASTL/string.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/ForwardRenderer/Passes/RayTracingModule.hh>

// https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html
// https://developer.nvidia.com/rtx/raytracing/dxr/dx12-raytracing-tutorial-part-1

// https://docs.vulkan.org/tutorial/latest/courses/18_Ray_tracing/00_Overview.html
// https://github.com/KhronosGroup/Vulkan-Tutorial/blob/main/attachments/38_ray_tracing.cpp

namespace mikoto::renderer {

    struct PathTracingInfo {
        FGTextureHandle mOutput{};
    };

    class PathTracingModule final {
    public:
        explicit PathTracingModule( rhi::RenderResolution resolution = rhi::RenderResolution::e1080P );

        // Exposes the transport module for explicit upload/build/trace/history registration.
        MKT_NODISCARD auto GetRayTracingModule() -> RayTracingModule&;

        auto SetAtmosphere( const AtmosphereData* data, const eastl::array<rhi::TextureHandle, 3>& textures ) -> void;
        auto SetBackground( SceneBackgroundType type, core::f32 blur ) -> void;

        auto SetEnvironmentFaces( const eastl::array<rhi::TextureHandle, 6>& textures ) -> void;

        auto Prepare(
            FrameGraph& graph,
            const scene::Scene* scene,
            const scene::Camera* camera,
            const RayTracingModule::Settings& settings,
            rhi::TextureHandle environment,
            float environmentScale, float exposure, float gamma ) -> void;

        auto ResetAccumulation() -> void;

        MKT_NODISCARD auto IsReady() const -> bool;
        MKT_NODISCARD auto GetSampleCount() const -> core::u32;
        MKT_NODISCARD auto GetError() const -> const eastl::string&;

    private:
        RayTracingModule mRayTracing{};
    };
}
#endif// MIKOTO_RENDERER_FORWARD_RENDERER_PATH_TRACING_MODULE_HH

