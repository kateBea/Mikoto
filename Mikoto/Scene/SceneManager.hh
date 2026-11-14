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

#ifndef MIKOTO_SCENE_SCENE_MANAGER_HH
#define MIKOTO_SCENE_SCENE_MANAGER_HH

#include <mutex>

#include <EASTL/vector.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/ReferenceCounted.hh>

#include <Scene/Scene.hh>

namespace mikoto::scene {

    class Scene;

    

    // Manages scenes currently active. Scenes are identified uniquely by their name for now.
    class SceneManager final : public core::IService, public core::Singleton<SceneManager> {
    public:
        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        // Opens a file dialog
        auto SaveToDisk( const Scene* scene ) -> void;
        auto Save( const Scene* scene, const filesystem::Path& path ) -> void;

        // Opens a file dialog
        MKT_NODISCARD auto LoadFromDisk() -> Scene*;
        MKT_NODISCARD auto Load( const filesystem::Path& path ) -> Scene*;

        MKT_NODISCARD auto CreateScene( eastl::string_view name ) -> Scene*;

        MKT_NODISCARD auto GetByName( eastl::string_view name ) -> Scene*;

        auto Unload( Scene* scene ) -> void;
        // Call on the editor thread, after pending imports have dropped their
        // scene handles. Scene destruction touches physics and animation state.
        auto CollectRetiredScenes() -> void;

    private:
        // [Internal usage]
        auto Register( eastl::string_view name, SceneHandle scene ) -> Scene*;

    private:
        SceneSerializer mSerializer{};
        ankerl::unordered_dense::map<eastl::string, SceneHandle> mScenes{};
        eastl::vector<SceneHandle> mRetiredScenes{};

        std::mutex mSceneRegisterMutex{};
    };
}// namespace mikoto::scene

#endif// MIKOTO_SCENE_SCENE_MANAGER_HH
