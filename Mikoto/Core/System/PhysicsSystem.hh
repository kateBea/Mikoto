//
// Created by zanet on 1/26/2025.
//

#ifndef PHYSICSSYSTEM_HH
#define PHYSICSSYSTEM_HH


#include <Core/Engine.hh>

namespace Mikoto {
    class PhysicSystem final : public IEngineSystem {
    public:
        explicit PhysicSystem(const EngineConfig& options) {

        }

        ~PhysicSystem() override = default;

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update() -> void override;
    };

}

#endif //PHYSICSSYSTEM_HH
