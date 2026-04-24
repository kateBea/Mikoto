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

#ifndef MIKOTO_EDITOR_APP_HH
#define MIKOTO_EDITOR_APP_HH

#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>

#include <ankerl/unordered_dense.h>

#include <Core/Engine.hh>
#include <Core/Application.hh>
#include <Core/EventSystem.hh>

#include <Platform/Window.hh>

#include <Theme/ThemeManager.hh>

namespace mikoto::editor {

    using namespace mikoto::core;
    using namespace mikoto::platform;

    class EditorApp final : public Application, public Subscriber {
    public:
        explicit EditorApp( Window* window );

        auto Init() -> void override;
        auto Run() -> void override;
        auto Shutdown() -> void override;
        auto Update() -> void override;

    private:
        auto InitEventCallbacks() -> void;

    private:
        Window* mWindow{};

        eastl::unique_ptr<Engine> mEngine{};
        eastl::unique_ptr<ThemeManager> mThemeManager{};
    };
}// namespace mikoto::editor

#endif// MIKOTO_EDITOR_APP_HH
