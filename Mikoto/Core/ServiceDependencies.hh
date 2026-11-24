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

#ifndef MIKOTO_CORE_SERVICE_DEPENDENCIES_HH
#define MIKOTO_CORE_SERVICE_DEPENDENCIES_HH

#include <concepts>
#include <typeindex>
#include <type_traits>

#include <EASTL/vector.h>

#include <Core/Service.hh>

// Dependency declarations only need names. Resolve() checks complete types at
// registration, avoiding inclusion of every backend through the Engine header.
namespace mikoto::animation {
    class AnimationSystem;
}
namespace mikoto::asset {
    class AssetRegistryService;
}
namespace mikoto::audio {
    class AudioService;
}
namespace mikoto::video { class VideoService; }
namespace mikoto::filesystem {
    class FileService;
    class FileWatcherService;
}// namespace mikoto::filesystem
namespace mikoto::network {
    class NetworkService;
}
namespace mikoto::physics {
    class PhysicSystem;
}
namespace mikoto::renderer {
    class RenderSystem;
    class ParticleSystem;
}// namespace mikoto::renderer
namespace mikoto::scene {
    class SceneManager;
}
namespace mikoto::scripting {
    class ScriptingService;
}
namespace mikoto::threading {
    class TaskService;
}

namespace mikoto::platform {
    class InputSystem;
    class EventSystem;
}

namespace mikoto::core {

    // TODO: This should not be here, I mean the service dependencies, every services in their own module need to specify who they depend on
    class LocalizationService;
    class SystemStats;
    class TimeService;

    template<typename Type>
    concept ServiceType = std::derived_from<Type, IService> && !std::is_abstract_v<Type>;

    // Types, not instances: constructing this list never constructs a service.
    // Resolution happens once when registering services, never per frame.
    template<typename... Dependencies>
    struct ServiceList {
        template<ServiceType Owner, bool Execution = false>
        static auto Resolve() -> eastl::vector<std::type_index> {
            MKT_STATIC_ASSERT( ( ServiceType<Dependencies> && ... ), "Dependencies must be concrete services" );
            MKT_STATIC_ASSERT( ( !std::same_as<Owner, Dependencies> && ... ), "A service cannot depend on itself" );

            if constexpr ( Execution ) {
                MKT_STATIC_ASSERT( ( std::derived_from<Dependencies, ISubsystem> && ... ), "UpdateAfter must name subsystems with Update()" );
            }

            return { std::type_index{ typeid( Dependencies ) }... };
        }
    };

    // Headless builds have no window input pump to wait for. Networking and
    // CPU statistics still update normally through the same scheduler.
#if MIKOTO_MODULE_RENDERER
    using InputUpdateDependencies = ServiceList<platform::InputSystem>;
#else
    using InputUpdateDependencies = ServiceList<>;
#endif

    // Initialization describes lifetime: prerequisites initialize first and
    // remain alive until this service shuts down. UpdateAfter only orders ticks;
    // it does not implicitly add a lifetime edge or make shared data thread-safe.
    template<typename InitializationDependencies = ServiceList<>, typename UpdateDependencies = ServiceList<>>
    struct ServiceDependencySet {
        using Initialization = InitializationDependencies;
        using UpdateAfter = UpdateDependencies;
    };

    template<typename Type>
    struct ServiceDependencies : ServiceDependencySet<> {
    };

    template<>
    struct ServiceDependencies<LocalizationService>
        : ServiceDependencySet<ServiceList<filesystem::FileService>> {
    };

    template<>
    struct ServiceDependencies<platform::InputSystem>
        : ServiceDependencySet<ServiceList<platform::EventSystem>> {
    };

    template<>
    struct ServiceDependencies<renderer::RenderSystem>
        : ServiceDependencySet<
                  ServiceList<filesystem::FileService, platform::EventSystem, platform::InputSystem>,
                  ServiceList<platform::InputSystem, animation::AnimationSystem, physics::PhysicSystem,
                              renderer::ParticleSystem, scripting::ScriptingService, SystemStats
                              >> {
    };

    // Physics debug passes need the rendering device and shader library during
    // initialization and destruction; physics UPDATE still runs before rendering.
    template<>
    struct ServiceDependencies<physics::PhysicSystem>
#if MIKOTO_MODULE_ANIMATION
        // Ragdolls consume sampled poses and release animator bindings during physics shutdown.
        : ServiceDependencySet<ServiceList<renderer::RenderSystem, asset::AssetRegistryService, animation::AnimationSystem>,
                               ServiceList<animation::AnimationSystem>> {
#else
        : ServiceDependencySet<ServiceList<renderer::RenderSystem, asset::AssetRegistryService>,
                               InputUpdateDependencies> {
#endif
    };

    template<>
    struct ServiceDependencies<animation::AnimationSystem>
        : ServiceDependencySet<ServiceList<threading::TaskService>, InputUpdateDependencies> {
    };

    // Particle spawning reads the transforms and emitter state published by these services.
    template<>
    struct ServiceDependencies<renderer::ParticleSystem>
        : ServiceDependencySet<ServiceList<>,
                               ServiceList<physics::PhysicSystem, scripting::ScriptingService, animation::AnimationSystem>> {
    };

    template<>
    struct ServiceDependencies<scripting::ScriptingService>
        : ServiceDependencySet<ServiceList<filesystem::FileService, filesystem::FileWatcherService>,
                               InputUpdateDependencies> {
    };

    template<>
    struct ServiceDependencies<SystemStats>
        : ServiceDependencySet<ServiceList<>, InputUpdateDependencies> {
    };

    template<>
    struct ServiceDependencies<network::NetworkService>
        : ServiceDependencySet<ServiceList<>> {
    };

    template<>
    struct ServiceDependencies<asset::AssetRegistryService>
        : ServiceDependencySet<ServiceList<renderer::RenderSystem, filesystem::FileService,
                                           filesystem::FileWatcherService, threading::TaskService, audio::AudioService>> {
    };

    template<>
    struct ServiceDependencies<video::VideoService>
        : ServiceDependencySet<ServiceList<asset::AssetRegistryService, audio::AudioService, threading::TaskService>> {
    };

    // Destroy scenes while their assets, physics worlds, particles, animators, audio and
    // script handles still have live owners to unregister from.
    template<>
    struct ServiceDependencies<scene::SceneManager>
        : ServiceDependencySet<ServiceList<asset::AssetRegistryService, audio::AudioService,
                                           physics::PhysicSystem, renderer::ParticleSystem,
                                           animation::AnimationSystem, scripting::ScriptingService
#if MIKOTO_MODULE_VIDEO
                                           , video::VideoService
#endif
                                           >> {
    };
}// namespace mikoto::core

#endif// MIKOTO_CORE_SERVICE_DEPENDENCIES_HH
