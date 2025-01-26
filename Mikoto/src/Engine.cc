//
// Created by zanet on 1/26/2025.
//


#include <Core/Engine.hh>
#include <Core/Logging/Assert.hh>

namespace Mikoto {
    template<typename SystemType, typename... Args>
    auto Register(Registry_T& registry, Args&&... args ) -> void {
        auto typeName = typeid( SystemType ).hash_code();
        MKT_ASSERT( !registry.contains( typeName ), "Engine::Register - Error registering system more than once." );

        auto system{ CreateScope<SystemType>( std::forward<Args>( args )... ) };
        registry.emplace( typeName, std::move( system ) );
    }

    template<typename SystemType>
    auto Unregister(Registry_T& registry) -> void {
        const auto typeName{ typeid( SystemType ).hash_code() };

        if ( registry.contains( typeName ) ) {
            registry.erase( typeName );
        }
    }

    template<typename SystemType>
    auto Get(Registry_T& registry) -> SystemType* {
        const auto typeName{ typeid( SystemType ).hash_code() };

        if ( registry.contains( typeName ) ) {
            return std::dynamic_pointer_cast<SystemType>( registry[typeName].get() );
        }

        return nullptr;
    }

    auto Engine::Init(const ConfigOptions& options) -> void {
        Register<EventManager>();
        Register<TaskManager>();
        Register<InputManager>();
        Register<FileManager>();
        Register<GUIManager>();
        Register<PhysicsManager>();
        Register<AudioManager>();
        Register<RenderManager>();
        Register<AssetManager>();

        for (auto& system : systems) {
            system->Init();
        }
    }

    auto Engine::Shutdown() -> void {

    }

}