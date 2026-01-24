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

#ifndef MIKOTO_EDITOR_APP_HH
#define MIKOTO_EDITOR_APP_HH

#include <Common/Application.hh>
#include <Core/EventService.hh>
#include <Platform/Window.hh>

namespace Mikoto {

    // There's a set of models that are loaded at start
    // and made available for editor to use on scenes
    enum class PrefabModels {
        CUBE,
        SPHERE,
        CONE,
        CYLINDER,
        SPONZA,
    };

    class EditorApp final : public Application, public Subscriber {
    public:

        auto Run() -> void override;

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update() -> void override;

        auto SetWindow(Window* window) -> void;

        static auto GetPrefabUri(PrefabModels prefab) -> const std::string&;

    private:
        auto InitPrefabs() -> void;
        auto SetupEventCallbacks() -> void;

    private:

        Window* m_Window{};
        ankerl::unordered_dense::map<PrefabModels, std::string> m_PrefabModels{};
    };
}

#endif// MIKOTO_EDITOR_APP_HH
