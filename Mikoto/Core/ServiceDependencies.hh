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

#ifndef MIKOTO_SERVICE_DEPENDENCIES_HH
#define MIKOTO_SERVICE_DEPENDENCIES_HH

#include <typeindex>

#include <EASTL/vector.h>

// FIXME: This shit is messed up something like the following seems to be necessary if you flip the order you get error: fatal error C1189: #error :  WinSock.h has already been included
// https://stackoverflow.com/questions/9750344/boostasio-winsock-and-winsock-2-compatibility-issue
// https://youtu.be/D-PC-huX-l8?list=PLqCJpWy5Fohd3S7ICFXwUomYW0Wv67pDD
// #include <boost/asio.hpp>
// #include <windows.h>


#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/EventSystem.hh>
#include <Core/InputSystem.hh>
#include <Core/LocalizationService.hh>
#include <Core/RuntimeConsole.hh>
#include <Core/SystemStats.hh>
#include <Core/TimeService.hh>
#include <Animation/AnimationSystem.hh>
#include <Assets/AssetsService.hh>
#include <Audio/AudioService.hh>
#include <Filesystem/FileService.hh>
#include <Filesystem/FileWatcherService.hh>
#include <ImGui/ImGuiService.hh>
#include <Networking/NetworkService.hh>
#include <Physics/PhysicSystem.hh>
#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Core/Renderer.hh>
#include <Renderer/Particle/ParticleSystem.hh>
#include <Scene/SceneManager.hh>
#include <Scripting/ScriptingService.hh>
#include <Threading/TaskService.hh>

namespace mikoto::core {

    enum class DependencyType {
        eExecution,
        eInitialization
    };

    template <typename ServiceType>
    struct ServiceDependencies {

        MKT_NODISCARD auto operator()( DependencyType type ) -> eastl::vector<std::type_index> {
            return as<ServiceType*>(this)->operator()(type);
        };
    };

    // I think I will move these specializations to each service file?
    // I think I will move these specializations to each service file?
    // Not entirely sure for the time being they will stay here

    template <>
    struct ServiceDependencies<animation::AnimationSystem> {

        MKT_NODISCARD auto operator()( DependencyType type ) -> eastl::vector<std::type_index> {
            switch (type) {

                case DependencyType::eExecution:
                    return GetExecDependenciesImpl();
                case DependencyType::eInitialization:
                    return GetInitDependenciesImpl();
            }

            return {};
        };

        MKT_NODISCARD auto GetInitDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }

        MKT_NODISCARD auto GetExecDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }
    };

    template <>
    struct ServiceDependencies<TimeService> {

        MKT_NODISCARD auto operator()( DependencyType type ) -> eastl::vector<std::type_index> {
            switch (type) {

                case DependencyType::eExecution:
                    return GetExecDependenciesImpl();
                case DependencyType::eInitialization:
                    return GetInitDependenciesImpl();
            }

            return {};
        };

        MKT_NODISCARD auto GetInitDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }

        MKT_NODISCARD auto GetExecDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }
    };

    template <>
    struct ServiceDependencies<filesystem::FileService> {

        MKT_NODISCARD auto operator()( DependencyType type ) -> eastl::vector<std::type_index> {
            switch (type) {

                case DependencyType::eExecution:
                    return GetExecDependenciesImpl();
                case DependencyType::eInitialization:
                    return GetInitDependenciesImpl();
            }

            return {};
        };

        MKT_NODISCARD auto GetInitDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }

        MKT_NODISCARD auto GetExecDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }
    };

    template <>
    struct ServiceDependencies<audio::AudioService> {
        MKT_NODISCARD auto operator()( DependencyType type ) -> eastl::vector<std::type_index> {
            switch (type) {

                case DependencyType::eExecution:
                    return GetExecDependenciesImpl();
                case DependencyType::eInitialization:
                    return GetInitDependenciesImpl();
            }

            return {};
        };

        MKT_NODISCARD auto GetInitDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }

        MKT_NODISCARD auto GetExecDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }
    };

    template <>
    struct ServiceDependencies<core::RuntimeConsole> {
        MKT_NODISCARD auto operator()( DependencyType type ) -> eastl::vector<std::type_index> {
            switch (type) {

                case DependencyType::eExecution:
                    return GetExecDependenciesImpl();
                case DependencyType::eInitialization:
                    return GetInitDependenciesImpl();
            }

            return {};
        };

        MKT_NODISCARD auto GetInitDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }

        MKT_NODISCARD auto GetExecDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }
    };

    template <>
    struct ServiceDependencies<FileWatcherService> {
        MKT_NODISCARD auto operator()( DependencyType type ) -> eastl::vector<std::type_index> {
            switch (type) {

                case DependencyType::eExecution:
                    return GetExecDependenciesImpl();
                case DependencyType::eInitialization:
                    return GetInitDependenciesImpl();
            }

            return {};
        };

        MKT_NODISCARD auto GetInitDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }

        MKT_NODISCARD auto GetExecDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }
    };


    template <>
    struct ServiceDependencies<threading::TaskService> {
        MKT_NODISCARD auto operator()( DependencyType type ) -> eastl::vector<std::type_index> {
            switch (type) {

                case DependencyType::eExecution:
                    return GetExecDependenciesImpl();
                case DependencyType::eInitialization:
                    return GetInitDependenciesImpl();
            }

            return {};
        };

        MKT_NODISCARD auto GetInitDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }

        MKT_NODISCARD auto GetExecDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }
    };

    template <>
    struct ServiceDependencies<core::LocalizationService> {
        MKT_NODISCARD auto operator()( DependencyType type ) -> eastl::vector<std::type_index> {
            switch (type) {

                case DependencyType::eExecution:
                    return GetExecDependenciesImpl();
                case DependencyType::eInitialization:
                    return GetInitDependenciesImpl();
            }

            return {};
        };

        MKT_NODISCARD auto GetInitDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }

        MKT_NODISCARD auto GetExecDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }
    };

    template <>
    struct ServiceDependencies<EventSystem> {
        MKT_NODISCARD auto operator()( DependencyType type ) -> eastl::vector<std::type_index> {
            switch (type) {

                case DependencyType::eExecution:
                    return GetExecDependenciesImpl();
                case DependencyType::eInitialization:
                    return GetInitDependenciesImpl();
            }

            return {};
        };

        MKT_NODISCARD auto GetInitDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }

        MKT_NODISCARD auto GetExecDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }
    };

    template <>
    struct ServiceDependencies<InputSystem> {
        MKT_NODISCARD auto operator()( DependencyType type ) -> eastl::vector<std::type_index> {
            switch (type) {

                case DependencyType::eExecution:
                    return GetExecDependenciesImpl();
                case DependencyType::eInitialization:
                    return GetInitDependenciesImpl();
            }

            return {};
        };

        MKT_NODISCARD auto GetInitDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }

        MKT_NODISCARD auto GetExecDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }
    };

    template <>
    struct ServiceDependencies<network::NetworkSystem> {
        MKT_NODISCARD auto operator()( DependencyType type ) -> eastl::vector<std::type_index> {
            switch (type) {

                case DependencyType::eExecution:
                    return GetExecDependenciesImpl();
                case DependencyType::eInitialization:
                    return GetInitDependenciesImpl();
            }

            return {};
        };

        MKT_NODISCARD auto GetInitDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }

        MKT_NODISCARD auto GetExecDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }
    };

    template <>
    struct ServiceDependencies<renderer::ParticleSystem> {
        MKT_NODISCARD auto operator()( DependencyType type ) -> eastl::vector<std::type_index> {
            switch (type) {

                case DependencyType::eExecution:
                    return GetExecDependenciesImpl();
                case DependencyType::eInitialization:
                    return GetInitDependenciesImpl();
            }

            return {};
        };

        MKT_NODISCARD auto GetInitDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }

        MKT_NODISCARD auto GetExecDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }
    };

    template <>
    struct ServiceDependencies<scripting::ScriptingService> {
        MKT_NODISCARD auto operator()( DependencyType type ) -> eastl::vector<std::type_index> {
            switch (type) {

                case DependencyType::eExecution:
                    return GetExecDependenciesImpl();
                case DependencyType::eInitialization:
                    return GetInitDependenciesImpl();
            }

            return {};
        };

        MKT_NODISCARD auto GetInitDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }

        MKT_NODISCARD auto GetExecDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }
    };

    template <>
    struct ServiceDependencies<renderer::RenderSystem> {
        MKT_NODISCARD auto operator()( DependencyType type ) -> eastl::vector<std::type_index> {
            switch (type) {

                case DependencyType::eExecution:
                    return GetExecDependenciesImpl();
                case DependencyType::eInitialization:
                    return GetInitDependenciesImpl();
            }

            return {};
        };

        MKT_NODISCARD auto GetInitDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }

        MKT_NODISCARD auto GetExecDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{
                typeid(animation::AnimationSystem),
                typeid(physics::PhysicSystem),
                typeid(renderer::ParticleSystem),
                typeid(scripting::ScriptingService),
            };
        }
    };

    template <>
    struct ServiceDependencies<scene::SceneManager> {
        MKT_NODISCARD auto operator()( DependencyType type ) -> eastl::vector<std::type_index> {
            switch (type) {

                case DependencyType::eExecution:
                    return GetExecDependenciesImpl();
                case DependencyType::eInitialization:
                    return GetInitDependenciesImpl();
            }

            return {};
        };

        MKT_NODISCARD auto GetInitDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{
                typeid(renderer::RenderSystem)
            };
        }

        MKT_NODISCARD auto GetExecDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }
    };

    template <>
    struct ServiceDependencies<asset::AssetsService> {
        MKT_NODISCARD auto operator()( DependencyType type ) -> eastl::vector<std::type_index> {
            switch (type) {

                case DependencyType::eExecution:
                    return GetExecDependenciesImpl();
                case DependencyType::eInitialization:
                    return GetInitDependenciesImpl();
            }

            return {};
        };

        MKT_NODISCARD auto GetInitDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{
                typeid(renderer::RenderSystem),
                typeid(audio::AudioService)
            };
        }

        MKT_NODISCARD auto GetExecDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }
    };

    template <>
    struct ServiceDependencies<gui::ImGuiService> {
        MKT_NODISCARD auto operator()( DependencyType type ) -> eastl::vector<std::type_index> {
            switch (type) {

                case DependencyType::eExecution:
                    return GetExecDependenciesImpl();
                case DependencyType::eInitialization:
                    return GetInitDependenciesImpl();
            }

            return {};
        };

        MKT_NODISCARD auto GetInitDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{
                typeid(renderer::RenderSystem)
            };
        }

        MKT_NODISCARD auto GetExecDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }
    };

    template <>
    struct ServiceDependencies<SystemStats> {
        MKT_NODISCARD auto operator()( DependencyType type ) -> eastl::vector<std::type_index> {
            switch (type) {

                case DependencyType::eExecution:
                    return GetExecDependenciesImpl();
                case DependencyType::eInitialization:
                    return GetInitDependenciesImpl();
            }

            return {};
        };

        MKT_NODISCARD auto GetInitDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{
            };
        }

        MKT_NODISCARD auto GetExecDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }
    };

    template <>
    struct ServiceDependencies<physics::PhysicSystem> {
        MKT_NODISCARD auto operator()( DependencyType type ) -> eastl::vector<std::type_index> {
            switch (type) {

                case DependencyType::eExecution:
                    return GetExecDependenciesImpl();
                case DependencyType::eInitialization:
                    return GetInitDependenciesImpl();
            }

            return {};
        };

        MKT_NODISCARD auto GetInitDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{
            };
        }

        MKT_NODISCARD auto GetExecDependenciesImpl() -> eastl::vector<std::type_index> {
            return eastl::vector<std::type_index>{

            };
        }
    };

}// namespace mikoto

#endif//MIKOTO_SERVICE_DEPENDENCIES_HH
