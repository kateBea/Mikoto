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

#ifndef MIKOTO_CAMERA_PASS_HH
#define MIKOTO_CAMERA_PASS_HH

#include <utility>

#include <Scene/Camera.hh>
#include <Scene/Scene.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    class CameraPass {
    public:
        struct CameraParameters {
            Mat4F Projection{};
            Mat4F ViewMatrix{};
            Mat4F InverseProjection{};
            Mat4F InverseViewProjection{};

            Vec4F ViewPosition{};

            Vec2F PlaneBounds{}; // Near and far plane
            Vec2F ScreenDimensions{}; // Width and Height
        };

        explicit CameraPass(RenderResolution resolution);

        auto RegisterPasses( FrameGraph& graph ) -> void;

        auto SetCamera( const Camera* camera ) -> void;
        auto SetScene( const Scene* scene ) -> void;
        auto SetEquirectangularMap( TextureHandle texture2D ) -> void;

        MKT_NODISCARD auto IsBackground(SceneBackground background) const -> bool;

        MKT_NODISCARD auto GetBackground() const -> SceneBackground;
        MKT_NODISCARD auto GetResolution() const -> RenderResolution;
        MKT_NODISCARD auto GetDimensions() const -> std::pair<float, float>;

        MKT_NODISCARD auto GetCamera() const -> const Camera*;
        MKT_NODISCARD auto GetScene() const -> const Scene*;
        MKT_NODISCARD auto GetClearColor() const -> const Vec4F&;
        MKT_NODISCARD auto GetEquirectangularMap() -> TextureHandle;

    private:

        auto RegisterCameraSetupPass( FrameGraph& graph ) -> void;

    private:
        RenderResolution m_Resolution{};
        SceneBackground m_SceneBackground{};

        const Camera* m_Camera{};
        const Scene* m_Scene{};

        TextureHandle m_EquirectangularMap{};
        Vec4F m_ClearColor{ 0.2f, 0.2f, 0.1f, 1.0f };

        CameraParameters m_CameraParameters{};
    };
}


#endif // MIKOTO_CAMERA_PASS_HH
