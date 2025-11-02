//
// Created by kate on 10/22/25.
//

#ifndef MIKOTO_PHYSICS_BASE_HH
#define MIKOTO_PHYSICS_BASE_HH

// The Jolt headers don't include Jolt.h. Always include Jolt.h before including any other Jolt header.
// You can use Jolt.h in your precompiled header to speed up compilation.
#include <Jolt/Jolt.h>

// Jolt includes
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <Scene/Scene.hh>
#include <Scene/Entity.hh>
#include <Scene/Component.hh>

namespace Mikoto {

    static constexpr Vec3F EARTH_GRAVITY{ 0.0f, -9.81f, 0.0f };

    // Controls physics API specifics
    class PhysicsBase final : public IService, public Singleton<PhysicsBase> {
    public:
        explicit PhysicsBase(Scene *scene);

        ~PhysicsBase() override = default;

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update(float timeStep) -> void override;

        auto SetSimulationScene(Scene* scene) -> void;

        auto SetGravity(const Vec3F& gravity) -> void;

        auto OnRigidBodyRemoved(RigidBodyComponent& rb) -> void;
        auto OnRigidBodyAdded(Entity& entity, RigidBodyComponent& rb) -> void;
        auto OnRigidBodyAdded(TransformComponent& tc, RigidBodyComponent& rb) -> void;

    private:
        auto PreUpdate() -> void;
        auto PostUpdate() -> void;

        MKT_NODISCARD static auto ToMat4F(const JPH::RMat44& jphMat) -> glm::mat4;
        MKT_NODISCARD static auto ToVec3F(const JPH::Vec3& jphVec3) -> glm::vec3;
        MKT_NODISCARD static auto ToQuatF(const JPH::Quat& jphQuat) -> glm::quat;

        MKT_NODISCARD static auto ToVec3(const glm::vec3& vec3GLM) -> JPH::Vec3;
        MKT_NODISCARD static auto ToQuat(const glm::vec3& vec3EulerAnglesGLM) -> JPH::Quat;

    private:
        Scene* m_Scene{ nullptr };
        Vec3F m_Gravity{ EARTH_GRAVITY };
    };
}

#endif
