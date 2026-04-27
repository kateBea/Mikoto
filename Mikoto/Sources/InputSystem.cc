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

// I tried to include any, but it
// is not necessary I don't know why
// CLion flags it as an unnecessary include
//#include <EASTL/any.h>

#include <EASTL/vector.h>
#include <EASTL/string.h>
#include <EASTL/utility.h>

#include <GLFW/glfw3.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/MouseCodes.hh>
#include <Core/Profiler.hh>
#include <Core/InputSystem.hh>

#include <Memory/Allocator.hh>

#include <Logging/Logger.hh>

namespace mikoto::core {

    using namespace mikoto::platform;

#if defined( USE_GLFW_INPUT )

    auto InputSystem::IsKeyPressed( const KeyCode keyCode ) const -> bool {
        bool result{ false };

        try {
            const auto window{ eastl::any_cast<GLFWwindow*>( mHandle->GetNativeWindow() ) };
            const auto state{ glfwGetKey( window, keyCode ) };

            result = state == GLFW_PRESS;
        } catch ( const std::exception& exception ) {
            MKT_CORE_LOGGER_ERROR( "InputManager - {}", exception.what() );
        }

        return result;
    }

    auto InputSystem::IsKeyReleased( const KeyCode keyCode ) const -> bool {
        bool result{ false };

        try {
            const auto window{ eastl::any_cast<GLFWwindow*>( mHandle->GetNativeWindow() ) };
            const auto state{ glfwGetKey( window, keyCode ) };

            result = state == GLFW_RELEASE;
        } catch ( const std::exception& exception ) {
            MKT_CORE_LOGGER_ERROR( "InputManager - {}", exception.what() );
        }

        return result;
    }

    auto InputSystem::IsMouseKeyPressed( const MouseButton button ) const -> bool {
        bool result{ false };

        try {
            const auto window{ eastl::any_cast<GLFWwindow*>( mHandle->GetNativeWindow() ) };
            const auto state{ glfwGetMouseButton( window, button ) };

            result = state == GLFW_PRESS;
        } catch ( const std::exception& exception ) {
            MKT_CORE_LOGGER_ERROR( "InputManager - {}", exception.what() );
        }

        return result;
    }

    auto InputSystem::IsMouseKeyReleased( const MouseButton button ) const -> bool {
        bool result{ false };

        try {
            const auto window{ eastl::any_cast<GLFWwindow*>( mHandle->GetNativeWindow() ) };
            const auto state{ glfwGetMouseButton( window, button ) };

            result = state == GLFW_RELEASE;
        } catch ( const std::exception& exception ) {
            MKT_CORE_LOGGER_ERROR( "InputManager - {}", exception.what() );
        }

        return result;
    }

    auto InputSystem::GetMousePos() const -> eastl::pair<double, double> {
        double posX{};
        double posY{};

        try {
            const auto window{ eastl::any_cast<GLFWwindow*>( mHandle->GetNativeWindow() ) };
            glfwGetCursorPos( window, MKT_ADDRESSOF( posX ), MKT_ADDRESSOF( posY ) );
        } catch ( const std::exception& exception ) {
            MKT_CORE_LOGGER_ERROR( "InputManager - {}", exception.what() );
        }

        return eastl::make_pair( posX, posY );
    }

    auto InputSystem::GetClipBoardContents() const -> eastl::string {
        try {
            const auto window{ eastl::any_cast<GLFWwindow*>( mHandle->GetNativeWindow() ) };
            const char* contents{ glfwGetClipboardString(window) };
            return contents ? eastl::string{ contents } : eastl::string{};
        } catch ( const std::exception& exception ) {
            MKT_CORE_LOGGER_ERROR( "InputManager - {}", exception.what() );
        }

        return eastl::string{};
    }

    auto InputSystem::GetMouseX() const -> double {
        const auto [mouseX, mouseY]{ GetMousePos() };
        return mouseX;
    }

    auto InputSystem::GetMouseY() const -> double {
        const auto [mouseX, mouseY]{ GetMousePos() };
        return mouseY;
    }

#endif

    InputSystem::InputSystem( const InputServiceCreateInfo& options )
        : mHandle{ options.mWindow } {
        MKT_ASSERT( mHandle, "Handle for InputService cannot be null." );
    }

    auto InputSystem::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO("Initializing InputService...");

        mIsInitialized = true;
    }

    auto InputSystem::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!mIsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down InputService..." );

        mHandle = nullptr;
    }

    auto InputSystem::Update( float ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( mHandle != nullptr ) {
            mHandle->ProcessEvents();
        }
    }

    auto InputSystem::SetFocus( Window* newHandle ) -> void {
        if ( newHandle ) {
            mHandle = newHandle;
            MKT_CORE_LOGGER_INFO( "InputManager - Input focus switch to '{}'", mHandle->GetTitle() );
        } else {
            MKT_CORE_LOGGER_WARN( "InputManager - Attempted to set focus for input handling on null window handle." );
        }
    }
}