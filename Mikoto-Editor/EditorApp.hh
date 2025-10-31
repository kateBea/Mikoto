/**
 * EngineSystem.hh
 * Created by kate on 6/7/23.
 * */

#ifndef MIKOTO_EDITOR_RUNNER_HH
#define MIKOTO_EDITOR_RUNNER_HH

// Project Headers
#include <Assets/Audio.hh>
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

        static auto GetPrefabUri(PrefabModels prefab) -> const std::string&;

    protected:
        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update() -> void override;

    private:
        auto InitPrefabs() -> void;
        auto SetupEventCallbacks() -> void;

    private:

        Unique<Window> m_Window{};
        LayerStack m_LayerStack{};

        // These path will be
        ankerl::unordered_dense::map<PrefabModels, std::string> m_PrefabModels{};
    };
}

#endif// MIKOTO_EDITOR_RUNNER_HH
