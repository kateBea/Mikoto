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

#include <memory>

#include <ankerl/unordered_dense.h>

#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Subsystem.hh>

namespace mikoto::scene {
    class Scene;
}

namespace mikoto::physics {

    class PhysicsWorld;

    using namespace mikoto::core;

    struct PhysicServiceCreateInfo {
        u32 mMaxBodies{ 1024 }; // Max simulation bodies
    };

    struct PhysicsWorldCreateInfo {
        scene::Scene* mScene{ nullptr };
        float3 mGravity{ 0.0f, -9.81f, 0.0f };
    };

    class PhysicSystem final : public core::ISubsystem, public core::Singleton<PhysicSystem> {
    public:
        explicit PhysicSystem(const PhysicServiceCreateInfo& options);

        auto Initialize() -> void override;
        auto Shutdown() -> void override;
        auto Update(float dt) -> void override;

        auto SetSimulationTarget( scene::Scene* scene ) -> void;
        MKT_NODISCARD auto CreatePhysicsWorld(const PhysicsWorldCreateInfo& spec) -> PhysicsWorld*;

        ~PhysicSystem() override = default;

    private:

        PhysicsWorld* mActiveWorld{};
        ankerl::unordered_dense::map<scene::Scene*, eastl::unique_ptr<PhysicsWorld>> mWorlds{};
    };
}

#endif //MIKOTO_PHYSICS_SERVICE_HH
