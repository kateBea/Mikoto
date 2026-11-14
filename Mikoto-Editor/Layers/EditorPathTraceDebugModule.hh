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

#ifndef MIKOTO_LAYERS_EDITOR_PATH_TRACE_DEBUG_MODULE_HH
#define MIKOTO_LAYERS_EDITOR_PATH_TRACE_DEBUG_MODULE_HH

#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>

#include <Core/Layer.hh>

#include <Renderer/Core/SceneRenderer.hh>

#include <Scene/Scene.hh>
#include <Scene/SceneCamera.hh>

namespace mikoto::editor {
    // A self-contained Cornell room using the same module as the editor renderer.
    class EditorPathTraceDebugModule final : public core::ILayer {
    public:
        EditorPathTraceDebugModule();
        auto OnCreate() -> void override;
        auto OnDestroy() -> void override;
        auto OnUpdate( float timeStep ) -> void override;
        auto OnRender( float deltaTime ) -> void override;
        auto OnEvent( platform::Event& ) -> void override;

        MKT_NODISCARD auto GetName() const -> eastl::string_view override;
    private:
        eastl::string mName{};

        eastl::unique_ptr<scene::Scene> mScene{};
        eastl::unique_ptr<renderer::ISceneRenderer> mRenderer{};
        scene::SceneCamera mCamera{};
        eastl::string mError{};
        float mAngle{};
        float mExposure{ 1.0f };
    };
}
#endif// MIKOTO_LAYERS_EDITOR_PATH_TRACE_DEBUG_MODULE_HH

