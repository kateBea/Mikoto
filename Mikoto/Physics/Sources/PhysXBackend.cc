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

#include <EASTL/utility.h>

#include <PxPhysicsAPI.h>

#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Physics/PhysXBackend.hh>

namespace mikoto::physics {
    struct PhysXBackend::Runtime {
        physx::PxDefaultAllocator mAllocator{};
        physx::PxDefaultErrorCallback mErrorCallback{};

        physx::PxFoundation* mFoundation{};
        physx::PxPhysics* mPhysics{};

        ~Runtime() {
            if ( mPhysics ) {
                mPhysics->release();
            }

            if ( mFoundation ) {
                mFoundation->release();
            }
        }
    };

    PhysXBackend::PhysXBackend() = default;
    PhysXBackend::~PhysXBackend() = default;

    auto PhysXBackend::Initialize() -> void {
        MKT_BEGIN_PROFILER();

        if ( mRuntime ) {
            return;
        }

        // NVIDIA's HelloWorld sample shows the SDK creation and release order.
        // https://github.com/NVIDIA-Omniverse/PhysX/blob/ovphysx-0.5.11/physx/snippets/snippethelloworld/SnippetHelloWorld.cpp
        auto runtime{ eastl::make_unique<Runtime>() };
        runtime->mFoundation = PxCreateFoundation( PX_PHYSICS_VERSION, runtime->mAllocator, runtime->mErrorCallback );

        if ( !runtime->mFoundation ) {
            MKT_THROW_RUNTIME_ERROR( "Could not initialize the PhysX foundation." );
        }

        runtime->mPhysics = PxCreatePhysics( PX_PHYSICS_VERSION, *runtime->mFoundation, physx::PxTolerancesScale{} );

        if ( !runtime->mPhysics ) {
            MKT_THROW_RUNTIME_ERROR( "Could not initialize the PhysX SDK." );
        }

        mRuntime = eastl::move( runtime );
    }

    auto PhysXBackend::Shutdown() -> void {
        MKT_BEGIN_PROFILER();
        mRuntime.reset();
    }

    auto PhysXBackend::GetPhysics() const -> physx::PxPhysics* {
        return mRuntime ? mRuntime->mPhysics : nullptr;
    }
}
