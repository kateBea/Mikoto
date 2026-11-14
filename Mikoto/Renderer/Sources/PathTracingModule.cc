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

#include <Renderer/ForwardRenderer/Passes/PathTracingModule.hh>

namespace mikoto::renderer {

    PathTracingModule::PathTracingModule( rhi::RenderResolution resolution )
        : mRayTracing{ resolution } {
    }

    auto PathTracingModule::GetRayTracingModule() -> RayTracingModule& {
        return mRayTracing;
    }

    auto PathTracingModule::SetAtmosphere( const AtmosphereData* data,
        const eastl::array<rhi::TextureHandle, 3>& textures ) -> void {
        mRayTracing.SetAtmosphere( data, textures );
    }

    auto PathTracingModule::SetBackground( SceneBackgroundType type, core::f32 blur ) -> void {
        mRayTracing.SetBackground( type, blur );
    }

    auto PathTracingModule::SetEnvironmentFaces( const eastl::array<rhi::TextureHandle, 6>& textures ) -> void {
        mRayTracing.SetEnvironmentFaces( textures );
    }

    auto PathTracingModule::Prepare( FrameGraph& graph, const scene::Scene* scene, const scene::Camera* camera, const RayTracingModule::Settings& settings,
        renderer::rhi::TextureHandle environment, float environmentScale, float exposure, float gamma ) -> void {
        mRayTracing.Prepare( graph, scene, camera, settings, environment, environmentScale, exposure, gamma );
    }

    auto PathTracingModule::ResetAccumulation() -> void {
        mRayTracing.ResetAccumulation();
    }

    auto PathTracingModule::IsReady() const -> bool {
        return mRayTracing.IsReady();
    }

    auto PathTracingModule::GetSampleCount() const -> core::u32 {
        return mRayTracing.GetSampleCount();
    }

    auto PathTracingModule::GetError() const -> const eastl::string& {
        return mRayTracing.GetError();
    }
}
