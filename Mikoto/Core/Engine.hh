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

#ifndef MIKOTO_ENGINE_HH
#define MIKOTO_ENGINE_HH

#include <typeinfo>
#include <typeindex>

#include <EASTL/vector.h>
#include <EASTL/tuple.h>
#include <EASTL/utility.h>

#include <taskflow/taskflow.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Registry.hh>
#include <Core/Singleton.hh>
#include <Core/Subsystem.hh>

#include <Platform/Window.hh>

namespace mikoto::core {

    struct EngineDescription {
        bool mEnableAllServices{ false };
        bool mEnableAllSubsystems{ false };
        platform::Window* mWindow{ nullptr };
    };

    struct ServiceNode {
        IService* mService{ nullptr };
        bool mIsSubsystem{ false };

        eastl::vector<std::type_index> mExecutionDeps{};
        eastl::vector<std::type_index> mConstructDeps{};
    };

    class Engine final : public core::Singleton<Engine> {
    public:
        explicit Engine(const EngineDescription& config);

        auto Initialize() -> void;
        auto Shutdown() -> void;

        auto Update() -> void;

    public:
        DISABLE_COPY_AND_MOVE_FOR(Engine);

    private:

        template<typename ServiceType, typename... Args>
        auto PushService(Args&&... args) -> ServiceType* {
            auto* instance{ dc_cast<IService*>(new ServiceType{ eastl::forward<Args>(args)... }) };

            ServiceNode node{
                .mService = instance,
                .mExecutionDeps = ResolveExecDependencies<ServiceType>(),
                .mConstructDeps = ResolveInitDependencies<ServiceType>(),
            };

            if constexpr (std::derived_from<ServiceType, ISubsystem>) {
                node.mIsSubsystem = true;
            }

            mNodes[typeid(ServiceType)] = eastl::move(node);

            return as<ServiceType*>( instance );
        }

        template<typename ServiceType, typename... Args>
        auto PushMainThreadService(Args&&... args) -> ServiceType* {
            auto* instance{ dc_cast<IService*>(new ServiceType{ eastl::forward<Args>(args)... }) };

            ServiceNode node{
                .mService = instance,
                .mExecutionDeps = ResolveExecDependencies<ServiceType>(),
                .mConstructDeps = ResolveInitDependencies<ServiceType>(),
            };

            if constexpr (std::derived_from<ServiceType, ISubsystem>) {
                node.mIsSubsystem = true;
            }

            mMainThreadNodes[typeid(ServiceType)] = eastl::move(node);

            return as<ServiceType*>( instance );
        }

        template<typename ServiceType>
        MKT_NODISCARD auto ResolveInitDependencies() -> eastl::vector<std::type_index>;

        template<typename ServiceType>
        MKT_NODISCARD auto ResolveExecDependencies() -> eastl::vector<std::type_index>;

        auto BuildInitTasks() -> void;
        auto BuildShutdownTasks() -> void;
        auto BuildExecutionTasks() -> void;

    private:

        tf::Taskflow mInitTaskGraph{};
        tf::Taskflow mShutdownTaskGraph{};

        tf::Taskflow mExecTaskGraph{};

        tf::Executor mExecutor{};
        ankerl::unordered_dense::map<std::type_index, tf::Task> mInitTasks{};
        ankerl::unordered_dense::map<std::type_index, tf::Task> mShutdownTasks{};

        ankerl::unordered_dense::map<std::type_index, tf::Task> mExecTasks{};

        ankerl::unordered_dense::map<std::type_index, ServiceNode> mNodes{};
        ankerl::unordered_dense::map<std::type_index, ServiceNode> mMainThreadNodes{};

        bool mEnableAllServices{ false };
        bool mEnableAllSubsystems{ false };
        platform::Window* mWindow{ nullptr };
    };

}

#include <Core/Engine.inl>

#endif //MIKOTO_ENGINE_HH
