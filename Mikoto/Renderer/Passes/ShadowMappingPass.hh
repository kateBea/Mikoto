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

#ifndef MIKOTO_SHADOW_MAPPING_PASS_HH
#define MIKOTO_SHADOW_MAPPING_PASS_HH


#include <vector>
#include <cstdint>
#include <vector>
#include <optional>

#include <glm/glm.hpp>

#include <Scene/Scene.hh>

#include <Memory/Allocator.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/RenderUtility.hh>
#include <Renderer/Passes/MeshCulling.hh>

namespace mikoto::renderer {

    struct LightCameraInfo {
        Mat4F LightView{};
        Mat4F LightProjection{};
    };

    class ShadowMappingPass final {
    public:
        explicit ShadowMappingPass(RenderResolution resolution);

        auto SetScene( const Scene* scene) -> void;
        auto SetCamera( const Camera* camera ) -> void;
        auto SetMeshCulling( MeshCulling& culling ) -> void;
        auto RegisterPasses( FrameGraph& graph ) -> void;

    private:
        auto RegisterDirShadowMap( FrameGraph& graph ) -> void;
        auto RegisterPointShadowMap( FrameGraph& graph ) -> void;
        auto RegisterSpotShadowMap( FrameGraph& graph ) -> void;
        auto RegisterDebugViewsPass( FrameGraph& graph ) -> void;

    private:
        const Scene* m_Scene{};
        const Camera* m_Camera{};
        MeshCulling* m_MeshCullingPass{};

        // Directional light shadows
        LightCameraInfo m_DirectionalShadowMapCameraInfo{};

        RenderResolution m_Resolution{};
    };
}


#endif // MIKOTO_SHADOW_MAPPING_PASS_HH
