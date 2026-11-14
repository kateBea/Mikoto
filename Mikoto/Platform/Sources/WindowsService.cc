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

#include <utility>
#include <memory>
#include <exception>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Platform/Utility.hh>
#include <Platform/MainWindow.hh>
#include <Platform/WindowsService.hh>

namespace mikoto::platform {

    WindowsService::WindowsService( WindowsServiceCreateInfo const & ) {
    }

    auto WindowsService::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO( "Initializing WindowsService..." );

        if ( mIsInitialized ) {
            return;
        }

        InitWindowHandling();

        mIsInitialized = true;
    }

    auto WindowsService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mIsInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Shutting down WindowsService..." );

        for ( const auto &window: mWindows ) {
            window->Shutdown();
        }

        mWindows.clear();

        ShutdownWindowHandling();

        mIsInitialized = false;
    }

    auto WindowsService::Create( const WindowProperties &properties ) -> IWindow * {
        MKT_BEGIN_PROFILER_NAMED();

        IWindow *result{ nullptr };

        try {
            eastl::unique_ptr<IWindow> window{};

            window = eastl::make_unique<MainWindow>( properties );

            if ( window ) {
                window->Init();
                result = mWindows.emplace_back( eastl::move( window ) ).get();
            }
        } catch ( std::exception &e ) {
            MKT_CORE_LOGGER_ERROR( "Window service exception: {}", e.what() );
        }

        return result;
    }

    auto WindowsService::InitWindowHandling() -> void {
        MKT_BEGIN_PROFILER();
        InitializeWindowBackend();
    }

    auto WindowsService::ShutdownWindowHandling() -> void {
        MKT_BEGIN_PROFILER();
        ShutdownWindowBackend();
    }
}// namespace mikoto::platform
