//
// Created by zanet on 1/26/2025.
//

#ifndef ENGINE_HH
#define ENGINE_HH

#include <unordered_map>

#include <Common/ConfigLoader.hh>
#include <Core/System/EngineSystem.hh>

namespace Mikoto {
    class Engine final {
    public:
        using Registry_T = std::unordered_map<Size_T, Scope_T<IEngineSystem>>;

        static auto Init(const ConfigOptions& options) -> void;
        static auto Shutdown() -> void;

    private:

        static inline Registry_T s_Registry;
    };
}

#endif //ENGINE_HH
