//
// Created by kate on 10/30/25.
//

#ifndef SCENE_MANAGER_HH
#define SCENE_MANAGER_HH

#include <vector>
#include <string_view>

#include <Library/Utility/Types.hh>
#include <Scene/Scene.hh>

namespace Mikoto {
    class SceneManager {
    public:
        /**
         * Add a scene to the manager.
         *
         * Takes ownership of the provided scene and stores it internally. The
         * manager will be responsible for the lifetime of the scene after this
         * call.
         *
         * @param scene A unique pointer owning the scene to add.
         */
        auto AddScene( Unique<Scene> scene ) -> void;

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
        std::vector<Unique<Scene>> m_Scenes{};
    };
}// namespace Mikoto


#endif//
