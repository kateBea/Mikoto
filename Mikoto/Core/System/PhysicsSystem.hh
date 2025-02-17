//
// Created by zanet on 1/26/2025.
//

#ifndef PHYSICSSYSTEM_HH
#define PHYSICSSYSTEM_HH


#include <Core/Engine.hh>

namespace Mikoto {
    class PhysicsSystem final : public IEngineSystem {
    public:
        explicit PhysicsSystem(const EngineConfig& options) {

        }

        ~PhysicsSystem() override = default;

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update() -> void override;
    };

}

#endif //PHYSICSSYSTEM_HH
