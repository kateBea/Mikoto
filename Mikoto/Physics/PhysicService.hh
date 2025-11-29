//
// Created by zanet on 1/26/2025.
//

#ifndef PHYSICSSYSTEM_HH
#define PHYSICSSYSTEM_HH

#include <ankerl/unordered_dense.h>

#include <Common/Service.hh>
#include <Physics/PhysicsWorld.hh>
#include <Scene/Component.hh>
#include <Scene/Scene.hh>

namespace Mikoto {
    class Scene;

    struct PhysicServiceCreateInfo {
        Vec3F Gravity{ 0.0f, -9.81f, 0.0f };
        UInt32 MaxBodies{ 1024 };
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

#endif //PHYSICSSYSTEM_HH
