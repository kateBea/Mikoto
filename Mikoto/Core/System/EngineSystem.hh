//
// Created by zanet on 1/26/2025.
//

#ifndef ENGINE_HH
#define ENGINE_HH

#include <Common/ConfigLoader.hh>

namespace Mikoto {
    struct EngineSystemSpec {
        ConfigOptions Configurations{};
    };

    class IEngineSystem {
    public:
        virtual ~IEngineSystem() = default;

        virtual auto Init() -> void = 0;
        virtual auto Shutdown() -> void = 0;
        virtual auto Update() -> void = 0;
    };
}

#endif //ENGINE_HH
