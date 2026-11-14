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

#ifndef MIKOTO_RUNTIME_RUNTIME_LAYER_HH
#define MIKOTO_RUNTIME_RUNTIME_LAYER_HH

#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>

#include <Core/Layer.hh>

#include <Renderer/Core/SceneRenderer.hh>

#include <Scene/SceneCamera.hh>

#include <Project/Project.hh>

namespace mikoto::runtime {

    /**
     * @brief Runs the project's startup scene and renders its active Main Camera.
     * @details SceneManager owns the scene; this layer owns the renderer. OnDestroy
     * stops Play and releases GPU resources before unloading the scene.
     */
    class RuntimeLayer final : public core::ILayer {
    public:
        RuntimeLayer( platform::IWindow* window, project::Project& project );

        auto OnCreate() -> void override;
        auto OnDestroy() -> void override;
        auto OnUpdate( float timeStep ) -> void override;
        auto OnRender( float deltaTime ) -> void override;
        auto OnEvent( platform::Event& event ) -> void override;

        MKT_NODISCARD auto GetName() const -> eastl::string_view override;

        auto CancelInput() -> void;

    private:
        auto UpdateCamera() -> void;

        eastl::string mName{};

        platform::IWindow* mWindow{};
        project::Project* mProject{};
        scene::Scene* mScene{};
        scene::SceneCamera mCamera{};
        eastl::unique_ptr<renderer::ISceneRenderer> mRenderer{};
        bool mPointerDown{};
        bool mCanvasKeysDown[7]{};
    };
}// namespace mikoto::runtime

#endif// MIKOTO_RUNTIME_RUNTIME_LAYER_HH
