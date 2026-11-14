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

#include <Platform/Event.hh>

#if MIKOTO_MODULE_RENDERER
#include <imgui.h>
#include <SDL3/SDL.h>
#include <imgui_impl_sdl3.h>

#include <EASTL/string_view.h>

#include <Platform/Utility.hh>
#include <Platform/MainWindow.hh>
#include <Platform/CoreEvents.hh>
#include <Platform/EventSystem.hh>
#include <Platform/InputSystem.hh>
#endif

namespace mikoto::platform {

    using namespace mikoto::core;

#if MIKOTO_MODULE_RENDERER
    auto PollWindowEvents() -> void {
        MKT_BEGIN_PROFILER();
        SDL_Event event{};

        while ( SDL_PollEvent( &event ) ) {
            // Feed SDL-backed ImGui contexts events for detached viewports too.
            // https://github.com/ocornut/imgui/blob/v1.92.9b-docking/backends/imgui_impl_sdl3.cpp
            if ( ImGui::GetCurrentContext() ) {
                const auto& io{ ImGui::GetIO() };
                if ( io.BackendPlatformUserData && io.BackendPlatformName &&
                     eastl::string_view{ io.BackendPlatformName }.starts_with( "imgui_impl_sdl3" ) ) {
                    ImGui_ImplSDL3_ProcessEvent( &event );
                }
            }

            if ( event.type == SDL_EVENT_QUIT ) {
                if ( platform::EventSystem::HasInstance() && platform::EventSystem::Get()->IsInitialized() ) {
                    platform::EventSystem::Get()->Queue<platform::WindowCloseEvent>();
                }

                continue;
            }

            if ( auto window{ SDL_GetWindowFromEvent( &event ) } ) {
                auto* owner{ static_cast<MainWindow *>( SDL_GetPointerProperty( SDL_GetWindowProperties( window ), kOwnerProperty, nullptr ) ) };

                // These events are not handled immediately, instead they are pushed
                // to the event queue to be handled next call to PollEvents of the Event Service
                if ( owner ) {
                    owner->PushEvent( event );
                }
            }
            else if ( InputSystem::HasInstance() ) {
                // Controller events carry a device ID and have no owning window.
                InputSystem::Get()->ProcessDeviceEvent( event );
            }
        }
    }

#endif

    Event::Event( const EventType type, const EventCategory categories )
        : mType{ type }, mCategories{ categories }, mHandled{ false } {
    }


    auto Event::IsType( const EventType type ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mType == type;
    }


    auto Event::GetCategoryFlags() const -> EventCategory {
        MKT_BEGIN_PROFILER();
        return mCategories;
    }


    auto Event::GetName() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return ToString();
    }


    auto Event::IsHandled() const -> bool {
        MKT_BEGIN_PROFILER();
        return mHandled;
    }


    auto Event::IsInCategory( EventCategory cat ) const -> bool {
        MKT_BEGIN_PROFILER();
        return GetCategoryFlags() & cat;
    }


    auto Event::SetHandled( const bool value ) -> void {
        MKT_BEGIN_PROFILER();
        mHandled = value;
    }


}
