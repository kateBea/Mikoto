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

#ifndef MIKOTO_PHYSICS_PHYSIC_SYSTEM_HH
#define MIKOTO_PHYSICS_PHYSIC_SYSTEM_HH

#include <EASTL/unique_ptr.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>

#include <Physics/PhysicsWorld.hh>
#include <Physics/PhysicsBackend.hh>

#include <Renderer/Core/PhysicsDebugRenderer.hh>

namespace mikoto::scene {
    class Scene;
}

namespace mikoto::physics {

    struct PhysicServiceCreateInfo {
        core::u32 mMaxBodies{ 1024 }; // Max simulation bodies
    };

    class PhysicSystem final : public core::ISubsystem, public core::Singleton<PhysicSystem> {
    public:
        explicit PhysicSystem(const PhysicServiceCreateInfo& options);

        auto Initialize() -> void override;
        auto Shutdown() -> void override;
        auto Update(float dt) -> void override;

        auto SetSimulationTarget( scene::Scene* scene ) -> void;
        auto RemovePhysicsWorld( scene::Scene* scene ) -> void;

        MKT_NODISCARD auto GetDebugRenderer() const -> renderer::PhysicsDebugRenderer*;
        MKT_NODISCARD auto GetDebugRendererSimple() const -> renderer::PhysicsDebugRendererSimple*;

        MKT_NODISCARD auto CreatePhysicsWorld(const PhysicsWorldCreateInfo& spec) -> PhysicsWorld*;
        MKT_NODISCARD auto GetPhysicsBackend() const -> const PhysicsBackend&;
        MKT_NODISCARD auto GetSimulationTarget() const -> scene::Scene *;

        ~PhysicSystem() override = default;

    private:
        auto InitializeDebugRenderer() -> void;

        PhysicsWorld* mActiveWorld{};

        PhysicsBackend mPhysicsBackend{};

        eastl::unique_ptr<renderer::PhysicsDebugRenderer> mPhysicsDebugRenderer{};
        eastl::unique_ptr<renderer::PhysicsDebugRendererSimple> mPhysicsDebugRendererSimple{};

        ankerl::unordered_dense::map<scene::Scene*, eastl::unique_ptr<PhysicsWorld>> mWorlds{};
    };
}

#endif// MIKOTO_PHYSICS_PHYSIC_SYSTEM_HH
