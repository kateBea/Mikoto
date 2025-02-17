//
// Created by zanet on 1/26/2025.
//

#ifndef ENGINE_HH
#define ENGINE_HH

#include <unordered_map>

#include <Common/ConfigLoader.hh>
#include <Common/Registry.hh>

namespace Mikoto {
    class IEngineSystem {
    public:
        virtual ~IEngineSystem() = default;

        virtual auto Init() -> void = 0;
        virtual auto Shutdown() -> void = 0;
        virtual auto Update() -> void = 0;
    };

    struct EngineConfig {
        ConfigOptions Options{};
        const Window* TargetWindow{};
    };

    class Engine final {
    public:

        template<typename SystemType>
        static auto GetSystem() -> SystemType& {
            auto systemPtr{ s_Registry.Get<SystemType>() };

            if (!systemPtr) {
                MKT_THROW_RUNTIME_ERROR("Engine::GetSystems - Error system not found.");
            }

            return *systemPtr;
        }

        static auto StartFrame() -> void;
        static auto EndFrame() -> void;

        static auto Init(const EngineConfig& options) -> void;
        static auto UpdateState() -> void;
        static auto Shutdown() -> void;

        static auto GetConfig() -> const EngineConfig& { return s_Options; }

    private:
        static inline EngineConfig s_Options{};
        static inline Registry<IEngineSystem> s_Registry;
    };
}

#endif //ENGINE_HH
