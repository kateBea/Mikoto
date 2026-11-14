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

#ifndef MIKOTO_CORE_ENGINE_HH
#define MIKOTO_CORE_ENGINE_HH

#include <thread>
#include <exception>

#include <EASTL/vector.h>
#include <EASTL/unique_ptr.h>

#include <ankerl/unordered_dense.h>

#include <Core/Types.hh>
#include <Core/Singleton.hh>
#include <Core/ServiceSchedule.hh>
#include <Core/LocalizationService.hh>
#include <Core/ServiceDependencies.hh>

#include <Platform/Window.hh>

namespace mikoto::core {

    struct EngineDescription {
        bool mEnableAllServices{ false };
        bool mEnableAllSubsystems{ false };
        platform::IWindow* mWindow{ nullptr };

        bool mEnableImGui{ true };
        bool mCompressTextures{ false };

        ISOLanguage mIsoLanguage{ ISOLanguage::EN_US };
    };

    class Engine final : public Singleton<Engine> {
    public:
        explicit Engine( const EngineDescription& config );

        auto Initialize() -> void;

        /** @brief Stops asynchronous producers and waits for outstanding jobs.
         *
         * Call on the owning thread before destroying application layers. Disables
         * engine updates, but retains service objects for dependent cleanup. Repeated
         * calls do not shut services down again; they still join outstanding jobs.
         * Service shutdown failures are retained and reported by Shutdown after cleanup.
         */
        auto StopBackgroundWork() -> void;

        /** @brief Stops background work and destroys services in dependency order.
         *
         * Call on the owning thread after releasing application resources. Calls
         * StopBackgroundWork internally, so engine-only shutdown is also supported.
         * Reports service shutdown failures after releasing all service objects.
         */
        auto Shutdown() -> void;

        auto Update( float timeStep ) -> void;

        // Join jobs before destroying layers/clients captured by background work.
        auto WaitForBackgroundTasks() -> void;

    private:
        struct ServiceNode {
            std::type_index mType{ typeid( void ) };

            ISubsystem* mSubsystem{};
            eastl::unique_ptr<IService> mService{};

            // Service dependencies
            eastl::vector<std::type_index> mUpdateAfter{};
            eastl::vector<std::type_index> mInitialization{};

            // Cleared when Shutdown is attempted; ownership lasts until the
            // dependency-ordered destruction pass, including on shutdown failure.
            bool mStarted{ false };
            bool mRunsOnMainThread{ false };
        };

        template<ServiceType Type, typename... Args>
            requires std::constructible_from<Type, Args...>
        auto PushService( bool mainThread, Args&&... args ) -> Type*;

        auto BuildSchedules() -> void;

        auto InitializeService( core::usize index ) -> void;
        auto ShutdownService( core::usize index ) -> void;

        auto CheckOwnerThread() const -> void;

        auto ClearServices() -> void;

        // Stable registration indices keep task callbacks independent of map
        // iteration/rehashing. Type lookup is only needed while building graphs.
        eastl::vector<ServiceNode> mNodes{};
        ankerl::unordered_dense::map<std::type_index, usize> mNodeIndices{};

        eastl::vector<std::exception_ptr> mShutdownErrors{};

        ServiceSchedule mInitialization{};
        ServiceSchedule mExecution{};
        ServiceSchedule mShutdown{};

        // Destroy the executor before the services captured by background work.
        tf::Executor mExecutor{};
        std::thread::id mOwnerThread{};

        f32 mTimeStep{};

        ISOLanguage mIsoLanguage{ ISOLanguage::EN_US };

        bool mInitialized{ false };
        bool mEnableAllServices{ false };
        bool mEnableAllSubsystems{ false };

        bool mEnableImGui{ true };
        bool mCompressTextures{ false };

        platform::IWindow* mWindow{};
    };
}// namespace mikoto::core
#include <Core/Engine.inl>

#endif// MIKOTO_CORE_ENGINE_HH
