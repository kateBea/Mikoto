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

#ifndef MIKOTO_PHYSICS_SERVICE_HH
#define MIKOTO_PHYSICS_SERVICE_HH

#include <ankerl/unordered_dense.h>

#include <Common/Service.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

#include <Physics/PhysicsWorld.hh>

namespace Mikoto {
    struct PhysicServiceCreateInfo {
        UInt32 MaxBodies{ 1024 }; // Max simulation bodies
        Vec3F Gravity{ PhysicsWorld::GetGravityFor( GravityBody::EARTH ) };
    };

    class PhysicService final : public IService, public Singleton<PhysicService> {
    public:
        explicit PhysicService(const PhysicServiceCreateInfo& options);

        ~PhysicService() override = default;

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update(float dt) -> void override;

        auto SetSimulationTarget( Scene* scene ) -> void;
        MKT_NODISCARD auto CreatePhysicsWorld(const PhysicsWorldCreateInfo& spec) -> PhysicsWorld*;

    private:

        PhysicsWorld* m_ActiveWorld{};
        ankerl::unordered_dense::map<Scene*, Unique<PhysicsWorld>> m_Worlds{};
    };
}

#endif //MIKOTO_PHYSICS_SERVICE_HH
