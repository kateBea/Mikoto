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

#include <Core/Profiler.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>
#include <Scene/Component.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Passes/CameraPass.hh>

namespace Mikoto {

    CameraPass::CameraPass( RenderResolution resolution )
        : m_Resolution{ resolution }
    {}

    auto CameraPass::RegisterPasses( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        RegisterCameraSetupPass( graph );
    }

    auto CameraPass::SetCamera( const Camera *camera ) -> void {
        m_Camera = camera;

        auto dimensions{ GetDimensions() };

        m_CameraParameters.Projection = m_Camera->GetProjection();
        m_CameraParameters.ViewMatrix = m_Camera->GetViewMatrix();
        m_CameraParameters.InverseProjection = glm::inverse( m_Camera->GetProjection() );

        m_CameraParameters.PlaneBounds = Vec2F{ m_Camera->GetNearPlane(), m_Camera->GetFarPlane() };
        m_CameraParameters.ScreenDimensions = Vec2F{ dimensions.first, dimensions.second };
        m_CameraParameters.ViewPosition = Vec4F{ m_Camera->GetPosition(), 1.0f };

    }

    auto CameraPass::SetEquirectangularMap( TextureHandle texture2D ) -> void {
        if (m_EquirectangularMap == texture2D) {
            return;
        }

        m_EquirectangularMap = texture2D;
    }
    
    auto CameraPass::SetScene( const Scene *scene ) -> void {
        m_Scene = scene;
    }

    auto CameraPass::IsBackground( SceneBackground background ) const -> bool {
        return m_SceneBackground == background;
    }

    auto CameraPass::GetBackground() const -> SceneBackground {
        return m_SceneBackground;
    }

    auto CameraPass::GetResolution() const -> RenderResolution {
        return m_Resolution;
    }

    auto CameraPass::GetDimensions() const -> std::pair<float, float> {
        return InferDimensions( m_Resolution );
    }

    auto CameraPass::GetScene() const -> const Scene * {
        return m_Scene;
    }

    auto CameraPass::GetClearColor() const -> const Vec4F& {
        return m_ClearColor;
    }

    auto CameraPass::GetEquirectangularMap() -> TextureHandle {
        return m_EquirectangularMap;
    }

    auto CameraPass::RegisterCameraSetupPass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "CameraInfoPass",
            []( FramePassBuilder &b ) {
                MKT_BEGIN_PROFILER_NAMED();

                b.CreateBuffer( "CameraInfoPass_CameraData", BufferUsage::UNIFORM,
                    MKT_SIZEOF( CameraParameters ), 1, ResourceUsageType::RESOURCE_USAGE_STREAMING );

                b.Write( "CameraInfoPass_CameraData", FrameResourceState::UniformBuffer );
            },
            [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                m_CameraParameters.InverseViewProjection = glm::inverse( m_CameraParameters.Projection * m_CameraParameters.ViewMatrix );

                ctx.UploadBuffer( "CameraInfoPass_CameraData", m_CameraParameters );
            },  FramePassNodeType::GENERIC );
    }

    auto CameraPass::GetCamera() const -> const Camera * {
        return m_Camera;
    }
}
