/**
 * EngineSystem.hh
 * Created by kate on 6/7/23.
 * */

#ifndef MIKOTO_EDITOR_RUNNER_HH
#define MIKOTO_EDITOR_RUNNER_HH

// Project Headers
#include <Assets/AudioClip.hh>
#include <Common/Application.hh>
#include <Core/Configuration.hh>
#include <Core/EventService.hh>
#include <Library/Utility/Types.hh>
#include <Platform/Window.hh>
#include <Core/LayerStack.hh>

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

        auto Run(Int32 argc, char** argv) -> Int32 override;

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

        // These path will be
        ankerl::unordered_dense::map<PrefabModels, std::string> m_PrefabModels{};
    };
}

#endif// MIKOTO_EDITOR_RUNNER_HH
