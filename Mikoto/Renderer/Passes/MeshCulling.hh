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

#ifndef MIKOTO_MESH_CULLING_HH
#define MIKOTO_MESH_CULLING_HH

#include <vector>

#include <glm/glm.hpp>

#include <Scene/Scene.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/RenderUtility.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Passes/ShaderParameteres.hh>

namespace Mikoto {

    struct MeshInstanceInfo {
        DrawIndexedState InstanceDrawState{};
        ankerl::unordered_dense::map<UInt64, bool> ActiveEntities{};
        ankerl::unordered_dense::map<UInt64, ShaderMaterialParams> InstanceInfos{};

        MKT_NODISCARD auto IsActive( UInt64 entityID ) const -> bool {
            bool result{ false };
            const auto it{ ActiveEntities.find( entityID ) };

            if ( it != ActiveEntities.end() ) {
                result = it->second;
            }

            return result;
        }

        auto Disable(UInt64 entityID )-> void {
            const auto it{ ActiveEntities.find( entityID ) };

            if ( it != ActiveEntities.end() ) {
                it->second = false;
            }
        }
    };

    class MeshCulling final {
    public:
        auto SetScene( Scene* scene) -> void;
        auto RegisterPasses( FrameGraph& graph ) -> void;
        auto DrawInstances( CommandContext& context ) -> void;

    private:
        auto UploadInstanceData( CommandContext& context ) -> void;
        auto SetupInstanceData( CommandContext& context ) -> void;

        auto RegisterMeshCullingPass(FrameGraph &graph) -> void;

    private:
        Scene* m_Scene{};
        Vec4F m_ClearColor{ 0.1f, 0.3f, 0.4f, 1.0f };

        std::vector<ShaderMaterialParams> m_Meshes{};
        ankerl::unordered_dense::map<MeshNode*, MeshInstanceInfo> m_MeshDrawState{};
    };

}

#endif // MIKOTO_MESH_CULLING_HH
