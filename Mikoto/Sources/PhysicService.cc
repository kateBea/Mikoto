//
// Created by zanet on 1/26/2025.
//

#include <utility>
#include <cstdarg>

#include <entt/entt.hpp>

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

#include <Logging/Logger.hh>
#include <Library/String/String.hh>
#include <Physics/PhysicService.hh>
#include <Scene/Scene.hh>
#include <Scene/Component.hh>
#include <Threading/ThreadUtility.hh>

namespace Mikoto {

    // Callback for traces, connect this to your own trace function if you have one
    static auto TraceImpl( const char *inFMT, ... ) -> void {
        // Format the message
        va_list list;
        va_start( list, inFMT );
        char buffer[1024];
        vsnprintf( buffer, sizeof( buffer ), inFMT, list );
        va_end( list );

        // TODO: Print to the TTY
        MKT_CORE_LOGGER_DEBUG( "" );
    }

    static auto ConvertToJoltMotionType(RigidBodyComponent::BodyType bodyType) -> JPH::EMotionType {
        switch (bodyType) {
            case RigidBodyComponent::BodyType::STATIC:
                return JPH::EMotionType::Static;
            case RigidBodyComponent::BodyType::KINEMATIC:
                return JPH::EMotionType::Kinematic;
            default:;
        }

        return JPH::EMotionType::Dynamic;
    }

    // Callback for asserts, connect this to your own assert handler if you have one
    static bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, UInt32 inLine) {
        // Print to the TTY
        MKT_CORE_LOGGER_DEBUG( "AssertFailedImpl" );

        // Breakpoint
        return true;
    };

    // Layer that objects can be in, determines which other objects it can collide with
    // Typically you at least want to have 1 layer for moving bodies and 1 layer for static bodies, but you can have more
    // layers if you want. E.g. you could have a layer for high detail collision (which is not used by the physics simulation
    // but only if you do collision testing).
    namespace Layers {
        static constexpr JPH::ObjectLayer NON_MOVING = 0;
        static constexpr JPH::ObjectLayer MOVING = 1;
        static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
    };

    // Each broadphase layer results in a separate bounding volume tree in the broad phase. You at least want to have
    // a layer for non-moving and moving objects to avoid having to update a tree full of static objects every frame.
    // You can have a 1-on-1 mapping between object layers and broadphase layers (like in this case) but if you have
    // many object layers you'll be creating many broad phase trees, which is not efficient. If you want to fine tune
    // your broadphase layers define JPH_TRACK_BROADPHASE_STATS and look at the stats reported on the TTY.
    namespace BroadPhaseLayers {
        static constexpr JPH::BroadPhaseLayer NON_MOVING( 0 );
        static constexpr JPH::BroadPhaseLayer MOVING( 1 );
        static constexpr UInt32 NUM_LAYERS( 2 );
    };

    /// Class that determines if two object layers can collide
    class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter {
    public:
        MKT_NODISCARD auto ShouldCollide( JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2 ) const -> bool override {
            switch (inObject1) {
                case Layers::NON_MOVING:
                    return inObject2 == Layers::MOVING;// Non moving only collides with moving
                case Layers::MOVING:
                    return true;// Moving collides with everything
                default:
                    JPH_ASSERT( false );
                    return false;
            }
        }
    };

    /// Class that determines if an object layer can collide with a broadphase layer
    class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
    public:
        MKT_NODISCARD auto ShouldCollide( JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2 ) const -> bool override {
            switch (inLayer1) {
                case Layers::NON_MOVING:
                    return inLayer2 == BroadPhaseLayers::MOVING;
                case Layers::MOVING:
                    return true;
                default:
                    JPH_ASSERT( false );
                    return false;
            }
        }
    };

    // BroadPhaseLayerInterface implementation
    // This defines a mapping between object and broadphase layers.
    class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
    public:
        BPLayerInterfaceImpl() {
            // Create a mapping table from object to broad phase layer
            mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
            mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
        }

        MKT_NODISCARD auto GetNumBroadPhaseLayers() const -> UInt32 override { return BroadPhaseLayers::NUM_LAYERS; }

        MKT_NODISCARD auto GetBroadPhaseLayer( JPH::ObjectLayer inLayer ) const -> JPH::BroadPhaseLayer override {
            JPH_ASSERT( inLayer < Layers::NUM_LAYERS );
            return mObjectToBroadPhase[inLayer];
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        MKT_NODISCARD auto GetBroadPhaseLayerName( JPH::BroadPhaseLayer inLayer ) const -> const char* override {
            switch (( JPH::BroadPhaseLayer::Type )inLayer) {
                case ( JPH::BroadPhaseLayer::Type )BroadPhaseLayers::NON_MOVING:
                    return "NON_MOVING";
                case ( JPH::BroadPhaseLayer::Type )BroadPhaseLayers::MOVING:
                    return "MOVING";
                default: JPH_ASSERT( false );
                    return "INVALID";
            }
        }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

    private:
        JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
    };

    struct Impl {
        JPH::PhysicsSystem PhysicsSystem{};
        JPH::BodyInterface* BodyInterface{ nullptr };

        Unique<JPH::TempAllocatorImpl> TempAllocator{};
        Unique<JPH::JobSystemThreadPool> JobSystem{};

        // Create mapping table from object layer to broadphase layer
        // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
        // Also have a look at BroadPhaseLayerInterfaceTable or BroadPhaseLayerInterfaceMask for a simpler interface.
        BPLayerInterfaceImpl BroadPhaseLayerInterface{};

        // Create class that filters object vs broadphase layers
        // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
        // Also have a look at ObjectVsBroadPhaseLayerFilterTable or ObjectVsBroadPhaseLayerFilterMask for a simpler interface.
        ObjectVsBroadPhaseLayerFilterImpl ObjectVsBroadPhaseLayerFilter{};

        // Create class that filters object vs object layers
        // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
        // Also have a look at ObjectLayerPairFilterTable or ObjectLayerPairFilterMask for a simpler interface.
        ObjectLayerPairFilterImpl ObjectLayerPairFilter{};
    };

    inline static Unique<Impl> s_Impl{};


    PhysicService::PhysicService(const PhysicServiceCreateInfo& options)
        : m_Gravity{ options.Gravity }
    {}

    auto PhysicService::Init() -> void {
        MKT_CORE_LOGGER_INFO("Initializing PhysicService...");

        // If you want your code to compile using single or double precision write
        // 0.0_r to get a Real value that compiles to double or float depending
        // if JPH_DOUBLE_PRECISION is set or not.
        using namespace JPH::literals;

        // Register allocation hook. In this example we'll just let Jolt use malloc / free
        // but you can override these if you want (see Memory.h).
        // This needs to be done before any other Jolt function is called.
        JPH::RegisterDefaultAllocator();

        // Install trace and assert callbacks
        JPH::Trace = TraceImpl;
        JPH_IF_ENABLE_ASSERTS( JPH::AssertFailed = AssertFailedImpl; )

        // Create a factory, this class is responsible for creating instances of classes
        // based on their name or hash and is mainly used for deserialization of saved data.
        // It is not directly used in this example but still required.
        JPH::Factory::sInstance = new JPH::Factory();

        // Register all physics types with the factory and install their collision handlers
        // with the CollisionDispatch class. If you have your own custom shape types you probably
        // need to register their handlers with the CollisionDispatch before calling this function.
        // If you implement your own default material (PhysicsMaterial::sDefault) make sure to
        // initialize it before this function or else this function will create one for you.
        JPH::RegisterTypes();

        s_Impl = CreateScope<Impl>();

        // We need a temp allocator for temporary allocations during the physics update. We're
        // pre-allocating 10 MB to avoid having to do allocations during the physics update.
        // B.t.w. 10 MB is way too much for this example but it is a typical value you can use.
        // If you don't want to pre-allocate you can also use TempAllocatorMalloc to fall back to
        // malloc / free.
        s_Impl->TempAllocator = CreateScope<JPH::TempAllocatorImpl>( 10 * 1024 * 1024 );

        // We need a job system that will execute physics jobs on multiple threads. Typically
        // you would implement the JobSystem interface yourself and let Jolt Physics run on top
        // of your own job scheduler. JobSystemThreadPool is an example implementation.
        s_Impl->JobSystem = CreateScope<JPH::JobSystemThreadPool>(
            JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, ThreadUtils::InferConcurrentThreads() - 1 );

        // This is the max amount of rigid bodies that you can add to the physics system. If you try to add more you'll get an error.
        // Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
        const UInt32 cMaxBodies{ 1024 };

        // This determines how many mutexes to allocate to protect rigid bodies from concurrent access. Set it to 0 for the default settings.
        const UInt32 cNumBodyMutexes{ 0 };

        // This is the max amount of body pairs that can be queued at any time (the broad phase will detect overlapping
        // body pairs based on their bounding boxes and will insert them into a queue for the narrowphase). If you make this buffer
        // too small the queue will fill up and the broad phase jobs will start to do narrow phase work. This is slightly less efficient.
        // Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
        const UInt32 cMaxBodyPairs{ 1024 };

        // This is the maximum size of the contact constraint buffer. If more contacts (collisions between bodies) are detected than this
        // number then these contacts will be ignored and bodies will start interpenetrating / fall through the world.
        // Note: This value is low because this is a simple test. For a real project use something in the order of 10240.
        const UInt32 cMaxContactConstraints{ 1024 };

        // Initialize physics system
        s_Impl->PhysicsSystem.Init(
            cMaxBodies,
            cNumBodyMutexes,
            cMaxBodyPairs,
            cMaxContactConstraints,

            // These need to be alive for as long as the physics system needs it
            s_Impl->BroadPhaseLayerInterface,
            s_Impl->ObjectVsBroadPhaseLayerFilter,
            s_Impl->ObjectLayerPairFilter );

        s_Impl->BodyInterface = std::addressof( s_Impl->PhysicsSystem.GetBodyInterface());

        s_Impl->PhysicsSystem.SetGravity(JPH::Vec3(m_Gravity.x, m_Gravity.y, m_Gravity.z));

        m_IsInitialized = true;

        // Tests
        // JPH::BoxShapeSettings boxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
        // auto shape = boxShape.Create().Get();
        //
        // JPH::BodyCreationSettings settings(
        //     shape,
        //     JPH::RVec3(0, 10, 0),
        //     JPH::Quat::sIdentity(),
        //     JPH::EMotionType::Dynamic,
        //     Layers::MOVING
        // );
        //
        // JPH::Body* body{ s_Impl->BodyInterface->CreateBody(settings) };
        // s_Impl->BodyInterface->AddBody(body->GetID(), JPH::EActivation::Activate);
        //
        // MKT_CORE_LOGGER_INFO("Total bodies: {}", s_Impl->PhysicsSystem.GetNumBodies());
        // MKT_CORE_LOGGER_INFO("Active rigid bodies: {}", s_Impl->PhysicsSystem.GetNumActiveBodies(JPH::EBodyType::RigidBody));
    }

    auto PhysicService::Shutdown() -> void {
        if (!m_IsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down PhysicService..." );

        s_Impl.reset();
        JPH::UnregisterTypes();

        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;

        m_IsInitialized = false;
    }

    auto PhysicService::Update(float dt) -> void {
        // ECS -> Jolt
        SyncTransformsToPhysics();

        constexpr int collisionSteps{ 1 };
        s_Impl->PhysicsSystem.Update(dt, collisionSteps, s_Impl->TempAllocator.get(), s_Impl->JobSystem.get());

        // Jolt -> ECS
        SyncTransformsFromPhysics();
    }

    auto PhysicService::SetSimulationScene( Scene *scene ) -> void {
        if (scene) {
            // This needs to be handled properly
            m_Scene = scene;
        }
    }

    auto PhysicService::SyncTransformsToPhysics() -> void {
        auto& registry{ m_Scene->GetRegistry() };

        for (auto [entity, rb, tr] : registry.view<RigidBodyComponent, TransformComponent>().each()) {
            if (rb.GetInternalBodyHandle() == nullptr || rb.IsDynamic())
                continue; // only update kinematic/static ones

            auto* body{ reinterpret_cast<JPH::Body*>(rb.GetInternalBodyHandle()) };
            s_Impl->BodyInterface->SetPositionAndRotation(
                body->GetID(),
                JPH::Vec3(tr.GetTranslation().x, tr.GetTranslation().y, tr.GetTranslation().z),
                JPH::Quat(tr.GetRotation().x, tr.GetRotation().y, tr.GetRotation().z, 1.0f/*tr.GetRotation().w*/),
                JPH::EActivation::DontActivate
            );
        }
    }

    auto PhysicService::SyncTransformsFromPhysics() -> void {
        auto& registry{ m_Scene->GetRegistry() };

        for (auto [entity, rb, tr] : registry.view<RigidBodyComponent, TransformComponent>().each()) {
            if (rb.GetInternalBodyHandle() == nullptr || !rb.IsDynamic())
                continue;

            const auto* body{ reinterpret_cast<JPH::Body*>(rb.GetInternalBodyHandle()) };
            const JPH::RMat44 transform{ s_Impl->BodyInterface->GetCenterOfMassTransform(body->GetID()) };

            const JPH::Vec3 pos{ transform.GetTranslation() };
            const JPH::Quat rot{ transform.GetRotation().GetQuaternion() };

            tr.SetTranslation(Vec3F(pos.GetX(), pos.GetY(), pos.GetZ()));
            tr.SetRotation(glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ()));
        }
    }

    auto PhysicService::OnRigidBodyRemoved( RigidBodyComponent &rb ) -> void {
        if (rb.GetInternalBodyHandle() == nullptr) {
            return;
        }

        const auto body{ reinterpret_cast<JPH::Body*>(rb.GetInternalBodyHandle()) };
        s_Impl->BodyInterface->RemoveBody(body->GetID());
        s_Impl->BodyInterface->DestroyBody(body->GetID());

        rb.RemoveBodyHandle();
    }

    auto PhysicService::OnRigidBodyAdded( Entity &entity, RigidBodyComponent &rb ) -> void {
        if (!s_Impl || !s_Impl->BodyInterface) {
            return;
        }

        // Convert transform to Jolt space
        auto& tr { entity.GetComponent<TransformComponent>() };
        JPH::Vec3 pos(tr.GetTranslation().x, tr.GetTranslation().y, tr.GetTranslation().z);
        //JPH::Quat rot(tr.GetRotation().x, tr.GetRotation().y, tr.GetRotation().z, 1.0f/*tr.GetRotation().w*/);

        // Simple shape for now (box)
        const JPH::BoxShapeSettings shapeSettings(JPH::Vec3(0.5f, 0.5f, 0.5f));
        auto shape{ shapeSettings.Create().Get() };

        JPH::BodyCreationSettings settings(
            shape,
            pos,
            JPH::Quat::sIdentity(),
            ConvertToJoltMotionType( rb.GetBodyType() ),
            Layers::MOVING
        );

        settings.mFriction = rb.GetFriction();
        settings.mMassPropertiesOverride.mMass = rb.GetMass();

        JPH::Body* body{ s_Impl->BodyInterface->CreateBody(settings) };
        s_Impl->BodyInterface->AddBody(body->GetID(), JPH::EActivation::Activate);

        // Now you can interact with the dynamic body, in this case we're going to give it a velocity.
        // (note that if we had used CreateBody then we could have set the velocity straight on the body before adding it to the physics system)
        //s_Impl->BodyInterface->SetLinearVelocity(body->GetID(), JPH::Vec3(0.0f, -2.0f, 0.0f));
        s_Impl->BodyInterface->AddImpulse(body->GetID(), JPH::Vec3(0.0f, -2.0f, 0.0f));

        rb.SetInternalBodyHandle( reinterpret_cast<std::uintptr_t*>(body) );

        MKT_CORE_LOGGER_INFO("Total bodies: {}", s_Impl->PhysicsSystem.GetNumBodies());
        MKT_CORE_LOGGER_INFO("Active rigid bodies: {}", s_Impl->PhysicsSystem.GetNumActiveBodies(JPH::EBodyType::RigidBody));
    }

}
