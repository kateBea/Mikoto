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

    struct InfiniteGridProperties {
        Vec4F OuterSquareColor{ 0.8f, 0.8f, 0.8f, 0.8f };
        Vec4F InnerSquareColor{ 0.8f, 0.8f, 0.8f, 0.8f };

        Vec4F XAxisColor{ 0.0, 0.0, 1.0, 1.0 };
        Vec4F ZAxisColor{ 1.0, 0.0, 0.0, 1.0 };

        float OuterSquareWidth{ 0.5f };
        float InnerSquareWidth{ 1.0f }; 

        float XAxisWidth{ 6.0f }; 
        float ZAxisWidth{ 6.0f }; 
    };

    class DebugPasses {
    public:
        explicit DebugPasses( RenderResolution resolution);

        auto RegisterPasses(FrameGraph& graph) -> void;

        auto SetWireframeLineLineWidth(float value) -> void;
        auto SetWireframeLineColor(const Vec4F& color) -> void;
        auto SetWireframeLineLineClearColor(const Vec4F& color) -> void;

        auto SetMeshCulling( MeshCulling& culling ) -> void;

        // Infinite grid
        auto EnableInfiniteGrid( bool enable ) -> void;

        auto SetOuterSquareColor( const Vec4F& color ) -> void;
        auto SetInnerSquareColor( const Vec4F& color ) -> void;

        auto SetOuterSquareWidth( float width ) -> void;
        auto SetInnerSquareWidth( float width ) -> void;

        auto SetZAxisWidth( float width ) -> void;
        auto SetXAxisWidth( float width ) -> void;

        auto SetZAxisColor( const Vec4F& color ) -> void;
        auto SetXAxisColor( const Vec4F& color ) -> void;

        // Wireframe
        auto SetWireframeEnable( bool enable ) -> void;
        auto SetClearColor( const Vec4F& vec ) -> void;
        auto SetLinesColor( const Vec4F& color ) -> void;
        // Merges final color image with the draw lines [DEPRECATED]
        auto ShowColorImage( bool value ) -> void;

    private:
        auto RegisterWireFrame( FrameGraph& graph ) -> void;
        auto RegisterHelloTriangle( FrameGraph& graph ) -> void;
        auto RegisterSimpleCompute( FrameGraph& graph ) -> void;
        auto RegisterInfiniteGrid( FrameGraph& graph ) -> void;
        auto RegisterHelloCube( FrameGraph& graph ) -> void;
        auto RegisterHelloTexture( FrameGraph& graph ) -> void;
        auto RegisterBoneDebug( FrameGraph& graph ) -> void;

        auto RegisterDebugViewsPass( FrameGraph& graph ) -> void;

    private:
        struct WireframeParams {
            Vec4F WireframeLineColor{ 0.0f, 0.0f, 0.0f, 1.0f };
        };

    private:
        Vec4F m_ClearColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        Vec4F m_LinesColor{ 1.0f, 1.0f, 1.0f, 1.0f };

        MeshCulling* m_Culling{};

        // Wireframe
        bool m_RunWireframe{ false };
        float m_WireframeLineWidth{ 1.0f };
        WireframeParams m_WireframeParams{};
        Vec4F m_WireframeClearColor{ 1.0f, 1.0f, 1.0f, 1.0f };

        // Infinite grid
        bool m_EnableInfiniteGrid{ true };
        InfiniteGridProperties m_InfiniteGridProperties{};

        bool m_ShowColorImageWireframe{ false };
        RenderResolution m_Resolution{ RenderResolution::FHD_1080 };
    };
}

#endif //MIKOTO_DEBUG_PASSES_HH