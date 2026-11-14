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

#include <cstdio>
#include <cstdarg>

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/PhysicsSettings.h>

#if defined( MIKOTO_ENABLE_PHYSX )
#include <common/PxTolerancesScale.h>
#include <foundation/PxPhysicsVersion.h>
#endif

#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <Threading/Utility.hh>

#include <Physics/Utility.hh>
#include <Physics/PhysicsBackend.hh>

namespace mikoto::physics {

    // Route Jolt diagnostics through the engine logger.
    static auto TraceJolt( const char* format, ... ) -> void {
        MKT_BEGIN_PROFILER();

        std::va_list args{};
        va_start( args, format );

        char buffer[1024]{};
        std::vsnprintf( buffer, sizeof( buffer ), format, args );
        va_end( args );

        MKT_CORE_LOGGER_DEBUG( "Jolt Trace {}", buffer );
    }

    PhysicsBackend::~PhysicsBackend() {
        Shutdown();
    }

    auto PhysicsBackend::Initialize() -> void {
        MKT_BEGIN_PROFILER();

        if ( mJoltFactory ) {
            return;
        }

        try {
            InitializeJolt();
#if defined( MIKOTO_ENABLE_PHYSX )
            InitializePhysX();
#endif
        } catch ( ... ) {
            Shutdown();
            throw;
        }
    }

    auto PhysicsBackend::Shutdown() -> void {
        MKT_BEGIN_PROFILER();

#if defined( MIKOTO_ENABLE_PHYSX )
        ShutdownPhysX();
#endif

        mJoltJobSystem.reset();

        if ( mJoltFactory ) {
            JPH::UnregisterTypes();
            JPH::Factory::sInstance = nullptr;
            mJoltFactory.reset();
        }
    }

    auto PhysicsBackend::GetJoltJobSystem() const -> JPH::JobSystemThreadPool* {
        return mJoltJobSystem.get();
    }

    auto PhysicsBackend::InitializeJolt() -> void {
        // Jolt registers its allocator and factory before constructing scene worlds.
        // https://github.com/jrouwe/JoltPhysics/blob/v5.6.0/HelloWorld/HelloWorld.cpp
        JPH::RegisterDefaultAllocator();
        JPH::Trace = TraceJolt;
        JPH_IF_ENABLE_ASSERTS( JPH::AssertFailed = AssertFailedImpl; )

        mJoltFactory = eastl::make_unique<JPH::Factory>();
        JPH::Factory::sInstance = mJoltFactory.get();
        JPH::RegisterTypes();

        mJoltJobSystem = eastl::make_unique<JPH::JobSystemThreadPool>( JPH::cMaxPhysicsJobs,
            JPH::cMaxPhysicsBarriers, threading::GetThreadConcurrency() );
    }

#if defined( MIKOTO_ENABLE_PHYSX )
    auto PhysicsBackend::GetPhysX() const -> physx::PxPhysics* {
        return mPhysX;
    }

    auto PhysicsBackend::InitializePhysX() -> void {
        // The foundation borrows the callbacks until after the SDK has been released.
        // https://github.com/NVIDIA-Omniverse/PhysX/blob/ovphysx-0.5.11/physx/snippets/snippethelloworld/SnippetHelloWorld.cpp
        mPhysXFoundation = PxCreateFoundation( PX_PHYSICS_VERSION, mPhysXAllocator, mPhysXErrorCallback );
        if ( !mPhysXFoundation ) {
            MKT_THROW_RUNTIME_ERROR( "Could not initialize the PhysX foundation." );
        }

        mPhysX = PxCreatePhysics( PX_PHYSICS_VERSION, *mPhysXFoundation, physx::PxTolerancesScale{} );
        if ( !mPhysX ) {
            MKT_THROW_RUNTIME_ERROR( "Could not initialize the PhysX SDK." );
        }
    }

    auto PhysicsBackend::ShutdownPhysX() -> void {
        if ( mPhysX ) {
            mPhysX->release();
            mPhysX = nullptr;
        }

        if ( mPhysXFoundation ) {
            mPhysXFoundation->release();
            mPhysXFoundation = nullptr;
        }
    }
#endif
}
