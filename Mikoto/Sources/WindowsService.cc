//    Copyright 2025 ケイト
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

#include <memory>
#include <utility>

#include <Core/Profiler.hh>
#include <Core/Platform.hh>
#include <Logging/Logger.hh>

#include <Platform/MainWindow.hh>

#if defined( MIKOTO_PLATFORM_LINUX )
#include <Platform/LinuxWindow.hh>
#elif defined( MIKOTO_PLATFORM_WINDOWS )
#include <Platform/Win32Window.hh>
#endif

#include <Platform/WindowsService.hh>

namespace Mikoto {

    WindowsService::WindowsService( WindowsServiceCreateInfo const & ) {}

    auto WindowsService::Init() -> void {
        InitWindowHandling();

        m_IsInitialized = true;
    }

    auto WindowsService::Shutdown() -> void {
        if (!m_IsInitialized) {
            return;
        }

        for (const auto& window : m_Windows) {
            window->Shutdown();
        }

        m_Windows.clear();

        ShutdownWindowHandling();

        m_IsInitialized = false;
    }

    auto WindowsService::Create( const WindowProperties &properties ) -> Window * {
        Window* result{ nullptr };

        try {
            Unique<Window> window{};

#if defined( MIKOTO_PLATFORM_WINDOWS ) && defined( MKT_USE_WIN32_WINDOW )
            window = CreateScope<Win32Window>( properties );
#elif defined( MIKOTO_PLATFORM_LINUX ) && defined( MKT_USE_XCB_WINDOW )
            window = CreateScope<LinuxWindow>( properties );
#else
            window = CreateScope<MainWindow>( properties );
#endif

            result = m_Windows.emplace_back( std::move( window ) ).get();
            result->Init();
        } catch (std::exception &e) {
            MKT_CORE_LOGGER_ERROR( "Window service exception: {}", e.what() );
        }

        return result;
    }

    auto WindowsService::InitWindowHandling() -> void {
        MKT_BEGIN_PROFILER_NAMED();

#if defined( MIKOTO_PLATFORM_WINDOWS ) && defined( MKT_USE_WIN32_WINDOW )
        // Win32
#elif defined( MIKOTO_PLATFORM_LINUX ) && defined( MKT_USE_XCB_WINDOW )
        // XCB
#else
        const auto ret{ glfwInit() };
        MKT_ASSERT(ret == GLFW_TRUE, "Failed to initialized the GLFW library.");

        glfwSetErrorCallback([](Int32 errCode, const char* desc) -> void {
            MKT_CORE_LOGGER_ERROR("GLFW error code: {} Description: {}", errCode, desc);
        });
#endif
    }

    auto WindowsService::ShutdownWindowHandling() -> void {
        MKT_BEGIN_PROFILER_NAMED();

#if defined( MIKOTO_PLATFORM_WINDOWS ) && defined( MKT_USE_WIN32_WINDOW )
        // Win32
#elif defined( MIKOTO_PLATFORM_LINUX ) && defined( MKT_USE_XCB_WINDOW )
        // XCB
#else
        glfwTerminate();
#endif
    }
}