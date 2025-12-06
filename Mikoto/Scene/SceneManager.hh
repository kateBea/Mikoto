//
// Created by kate on 10/30/25.
//

#ifndef SCENE_MANAGER_HH
#define SCENE_MANAGER_HH

#include <ankerl/unordered_dense.h>

#include <Library/Utility/Types.hh>
#include <Scene/Scene.hh>
#include <Scene/SceneSerializer.hh>
#include <string_view>

#include "Common/Service.hh"

namespace Mikoto {

    class SceneManager final : public Singleton<SceneManager>, public IService {
    public:

        auto Init() -> void override;
        auto Shutdown() -> void override;

        // Prompts the user with a native
        // open dialog to select a file
        auto LoadSceneFromDisk(SceneSerializer* serializer) -> Scene*;
        auto SaveSceneFromDisk(Scene* scene, SceneSerializer* serializer) -> void;

        auto CreateScene( std::string_view name ) -> Scene*;

        /**
         * Retrieve a scene by name.
         *
         * Searches the scenes owned by the manager and returns a raw pointer to
         * the first scene whose name matches the provided string view. The
         * returned pointer is non-owning; the caller must not delete it. If no
         * matching scene is found, returns nullptr.
         *
         * @param name The name of the scene to search for.
         * @return A non-owning pointer to the Scene if found, otherwise nullptr.
         */
        MKT_NODISCARD auto GetByName( std::string_view name ) -> Scene *;

    private:
        // [Internal usage]
        auto RegisterNewScene(std::string_view name, Unique<Scene>&& scene ) -> Scene*;

    private:

        ankerl::unordered_dense::map<std::string, Unique<Scene>> m_Scenes{};
    };
}// namespace Mikoto


#endif//
