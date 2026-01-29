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

#ifndef MIKOTO_SCENE_MANAGER_HH
#define MIKOTO_SCENE_MANAGER_HH

#include <string_view>

#include <ankerl/unordered_dense.h>

#include <Scene/Scene.hh>
#include <Common/Service.hh>
#include <Library/Utility/Types.hh>
#include <Scene/SceneSerializer.hh>

namespace Mikoto {

    // Manages scenes currently active. Scenes are identified uniquely by their name for now.
    class SceneManager final : public Singleton<SceneManager>, public IService {
    public:

        auto Init() -> void override;
        auto Shutdown() -> void override;

        // Opens a file dialog
        auto LoadFromDisk() -> Scene*;
        auto Load(const Path& path) -> Scene*;

        // Opens a file dialog
        auto SaveToDisk( const Scene* scene) -> void;
        auto Save( const Scene* scene, const Path& path ) -> void;

        auto CreateScene( std::string_view name ) -> Scene*;

        MKT_NODISCARD auto GetByName( std::string_view name ) -> Scene *;

    private:
        // [Internal usage]
        auto Register(std::string_view name, Unique<Scene>&& scene ) -> Scene*;

    private:
        SceneSerializer m_Serializer{};
        ankerl::unordered_dense::map<std::string, Unique<Scene>> m_Scenes{};
    };
}

#endif // MIKOTO_SCENE_MANAGER_HH
