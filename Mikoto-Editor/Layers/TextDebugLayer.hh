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

#ifndef MIKOTO_EDITOR_TEXT_DEBUG_LAYER_HH
#define MIKOTO_EDITOR_TEXT_DEBUG_LAYER_HH

#include <EASTL/array.h>
#include <EASTL/string.h>

#include <Core/Layer.hh>

#include <Renderer/Core/SceneRenderer.hh>

#include <Scene/Scene.hh>
#include <Scene/SceneCamera.hh>

namespace mikoto::editor {
    /** Isolated Unicode/emoji scene comparing the production MSDF and Slug paths. */
    class TextDebugLayer final : public core::ILayer {
    public:
        TextDebugLayer();
        auto OnCreate() -> void override;
        auto OnDestroy() -> void override;
        auto OnUpdate( core::f32 timeStep ) -> void override;
        auto OnRender( float deltaTime ) -> void override;
        auto OnEvent( platform::Event& event ) -> void override;

        MKT_NODISCARD auto GetName() const -> eastl::string_view override;

    private:
        auto ReloadSample() -> void;
        auto UpdateText() -> void;

        eastl::string mName{};

        eastl::unique_ptr<scene::Scene> mScene{};
        eastl::unique_ptr<renderer::ISceneRenderer> mRenderer{};
        scene::SceneCamera mCamera{};
        eastl::array<renderer::FontHandle, 2> mFonts{};
        eastl::array<entt::entity, 2> mEntities{ entt::null, entt::null };
        eastl::string mError{};
        eastl::string mContents{};
        eastl::string mSample{};
        core::u32 mSampleLines{};
        core::i32 mFontIndex{};
        core::i32 mRepetitions{ 12 };
        core::f32 mFontSize{ 20.0f };
        core::f32 mScroll{};
        core::f32 mOpacity{ 1.0f };
        core::f32 mOutlineWidth{};
        core::float4 mOutlineColor{ 0.2f, 0.1f, 0.6f, 1.0f };
        bool mVisible{ true };
    };
}// namespace mikoto::editor

#endif

