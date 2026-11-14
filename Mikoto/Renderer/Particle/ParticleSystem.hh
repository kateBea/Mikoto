//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_RENDERER_PARTICLE_PARTICLE_SYSTEM_HH
#define MIKOTO_RENDERER_PARTICLE_PARTICLE_SYSTEM_HH

#include <ankerl/unordered_dense.h>

#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>

#include <Renderer/Particle/ParticleEmitter.hh>



namespace mikoto::scene {
    class Scene;
}
namespace mikoto::renderer {
    struct ParticleSystemCreateInfo {};

    // Scenes own their emitter instances. The service keeps non-owning scene
    // registrations and advances only scenes which are playing or simulating.
    class ParticleSystem final : public core::ISubsystem, public core::Singleton<ParticleSystem> {
    public:
        explicit ParticleSystem( const ParticleSystemCreateInfo& info );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        auto Update( float timeStep ) -> void override;

        /** @brief Selects the simulation backend for all scenes.
         * Instances restart on their next scene update. */
        auto SetSimulationMode( ParticleSimulationMode mode ) -> void;

        MKT_NODISCARD auto GetSimulationMode() const -> ParticleSimulationMode;

        /** @brief Registers a scene without taking ownership; repeated registration is harmless.
         * @param scene Scene to tick during Play or Simulate. Register outside the particle update.
         */
        auto RegisterScene( scene::Scene& scene ) -> void;

        /** @brief Removes a scene before it is destroyed or after its contents are cleared.
         * @param scene Previously registered scene. Removing an absent scene is harmless.
         */
        auto UnRegisterScene( scene::Scene& scene ) -> void;

    private:
        ParticleSimulationMode mSimulationMode{ ParticleSimulationMode::eCpu };

        // Non-owning registrations; scene lifecycle changes run outside the service tick.
        ankerl::unordered_dense::set<scene::Scene*> mScenes{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_PARTICLE_PARTICLE_SYSTEM_HH
