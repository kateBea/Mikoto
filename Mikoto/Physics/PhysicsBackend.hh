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

#ifndef MIKOTO_PHYSICS_PHYSICS_BACKEND_HH
#define MIKOTO_PHYSICS_PHYSICS_BACKEND_HH

#include <EASTL/unique_ptr.h>

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>

#if defined( MIKOTO_ENABLE_PHYSX )
#include <PxPhysics.h>
#include <foundation/PxFoundation.h>
#include <extensions/PxDefaultAllocator.h>
#include <extensions/PxDefaultErrorCallback.h>
#endif

#include <Core/Core.hh>

namespace mikoto::physics {

    // Owns the process-wide physics SDKs. Scene worlds must release their SDK resources before shutdown.
    class PhysicsBackend final {
    public:
        ~PhysicsBackend();

        auto Initialize() -> void;
        auto Shutdown() -> void;

        MKT_NODISCARD auto GetJoltJobSystem() const -> JPH::JobSystemThreadPool*;

#if defined( MIKOTO_ENABLE_PHYSX )
        // Borrows the PhysX SDK instance until Shutdown. Returns nullptr before initialization.
        MKT_NODISCARD auto GetPhysX() const -> physx::PxPhysics*;
#endif

    private:
        auto InitializeJolt() -> void;

#if defined( MIKOTO_ENABLE_PHYSX )
        auto InitializePhysX() -> void;
        auto ShutdownPhysX() -> void;
#endif

        eastl::unique_ptr<JPH::Factory> mJoltFactory{};
        eastl::unique_ptr<JPH::JobSystemThreadPool> mJoltJobSystem{};

#if defined( MIKOTO_ENABLE_PHYSX )
        physx::PxDefaultAllocator mPhysXAllocator{};
        physx::PxDefaultErrorCallback mPhysXErrorCallback{};
        physx::PxFoundation* mPhysXFoundation{};
        physx::PxPhysics* mPhysX{};
#endif
    };
}

#endif// MIKOTO_PHYSICS_PHYSICS_BACKEND_HH
