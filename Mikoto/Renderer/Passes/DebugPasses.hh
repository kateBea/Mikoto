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

#ifndef MIKOTO_DEBUG_PASSES_HH
#define MIKOTO_DEBUG_PASSES_HH


#include <Scene/Scene.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Passes/MeshCulling.hh>
#include <Renderer/Passes/ShaderParameteres.hh>

namespace Mikoto {

    class DebugPasses {
    public:
        explicit DebugPasses( RenderResolution resolution);

        auto RegisterPasses(FrameGraph& graph) -> void;

        auto SetWireframeLineLineWidth(float value) -> void;
        auto SetWireframeLineColor(const Vec4F& color) -> void;
        auto SetWireframeLineLineClearColor(const Vec4F& color) -> void;

        auto SetClearColor( const Vec4F& vec ) -> void;
        auto SetLinesColor( const Vec4F& color ) -> void;
        auto ShowColorImage( bool value ) -> void;
        auto SetMeshCulling( MeshCulling& culling ) -> void;
        auto SetWireframeEnable( bool enable ) -> void;

    private:
        auto RegisterWireFrame( FrameGraph& graph ) -> void;
        auto RegisterHelloTriangle( FrameGraph& graph ) -> void;
        auto RegisterSimpleCompute( FrameGraph& graph ) -> void;
        auto RegisterInfiniteGrid( FrameGraph& graph ) -> void;
        auto RegisterHelloCube( FrameGraph& graph ) -> void;
        auto RegisterHelloTexture( FrameGraph& graph ) -> void;

        auto RegisterDebugViewsPass( FrameGraph& graph ) -> void;

    private:
        struct WireframeParams {
            Vec4F WireframeLineColor{ 0.0f, 0.0f, 0.0f, 1.0f };
        };
    private:

        // Texture to be displayed in the Texture debug pass
        TextureHandle m_TextureHandle{};

        Vec4F m_ClearColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        Vec4F m_LinesColor{ 1.0f, 1.0f, 1.0f, 1.0f };

        MeshCulling* m_Culling{};

        // Wireframe
        WireframeParams m_WireframeParams{};
        Vec4F m_WireframeClearColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        float m_WireframeLineWidth{ 1.0f };
        bool m_RunWireframe{ false };

        bool m_ShowColorImageWireframe{ false };
        RenderResolution m_Resolution{ RenderResolution::FHD_1080 };
    };
}

#endif //MIKOTO_DEBUG_PASSES_HH