//
// Created by zanet on 1/26/2025.
//

#ifndef PHYSICSSYSTEM_HH
#define PHYSICSSYSTEM_HH

#include <Common/Service.hh>
#include <Scene/Entity.hh>
#include <Scene/Scene.hh>
#include <Scene/Component.hh>
#include <Physics/PhysicsBase.hh>

namespace Mikoto {
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

        auto SetSimulationScene(Scene* scene) -> void;

    private:
        friend class Scene;

        auto OnRigidBodyRemoved(RigidBodyComponent& rb) -> void;
        auto OnRigidBodyAdded(Entity& entity, RigidBodyComponent& rb) -> void;

        auto OnRigidBodyAdded(TransformComponent& t, RigidBodyComponent& rb) -> void;

        Unique<PhysicsBase> m_PhysicsBase{};
    };

}

#endif //PHYSICSSYSTEM_HH
