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

    class MeshCulling final {
    public:
        auto SetScene( Scene* scene) -> void;
        auto SetCamera( const Camera* camera ) -> void;
        auto RegisterPasses( FrameGraph& graph ) -> void;

        auto DrawInstances( CommandContext& context ) -> void;

    private:
        auto SetupInstanceData( CommandContext& context ) -> void;

        auto RegisterScatteredWrites(FrameGraph &graph) -> void;
        auto RegisterMeshCullingPass(FrameGraph &graph) -> void;

    private:
        Scene* m_Scene{};
        const Camera* m_Camera{};
        Vec4F m_ClearColor{ 0.1f, 0.3f, 0.4f, 1.0f };

        Size m_ObjectUpdateCount{};

        std::vector<UInt32> m_MeshInfoIndices{};
        std::vector<MeshParameters> m_MeshInfo{};

        std::vector<std::array<Mat4F, MAX_BONES_PER_MESH>> m_SkinnedMeshes{};
        ankerl::unordered_dense::map<MeshNode*, Size> m_MeshDrawInstanceCount{};
        ankerl::unordered_dense::map<MeshNode*, DrawIndexedState> m_DrawIndexedState{};
        ankerl::unordered_dense::map<MeshNode*, std::vector<ShaderMaterialParams>> m_InstanceInfos{};

        // Refactor material upload
        std::vector<ShaderMaterial> m_ShaderMaterialList{};
        ankerl::unordered_dense::map<UInt32, ShaderMaterial> m_ShaderMaterialPerDrawObject{};
    };

}

#endif // MIKOTO_MESH_CULLING_HH
