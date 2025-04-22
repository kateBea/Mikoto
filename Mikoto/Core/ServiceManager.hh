//
// Created by zanet on 1/26/2025.
//

#ifndef ENGINE_HH
#define ENGINE_HH

#include <Common/Configuration.hh>
#include <Common/Service.hh>
#include <Library/Data/Registry.hh>

namespace Mikoto {

    struct EngineConfig {
        Configuration Options{};
        Window* TargetWindow{};
    };

    class ServiceManager final {
    public:

        static auto Init(const EngineConfig& options) -> void;
        static auto Update() -> void;
        static auto Shutdown() -> void;

        static auto StartFrame() -> void;
        static auto EndFrame() -> void;

    private:
        template<typename SystemType>
        static auto GetService() -> SystemType& {
            auto systemPtr{ s_Registry.Get<SystemType>() };

            if (!systemPtr) {
                MKT_THROW_RUNTIME_ERROR("Engine::GetSystems - Error system not found.");
            }

            return *systemPtr;
        }

    private:
        static inline Registry<std::any> s_Registry{};
    };
}

#endif //ENGINE_HH
