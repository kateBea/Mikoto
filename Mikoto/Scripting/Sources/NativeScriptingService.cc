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

#include <chrono>
#include <fstream>
#include <filesystem>

#include <EASTL/unique_ptr.h>
#include <SDL3/SDL.h>

#include <Core/Exception.hh>
#include <Core/RuntimeConsole.hh>

#include <Logging/Logger.hh>

#include <Filesystem/FileService.hh>

#include <Scripting/Native/NativeScript.hh>
#include <Scripting/Native/NativeScriptingService.hh>

namespace mikoto::scripting {

    static constexpr core::f32 kSourceCheckIntervalSeconds{ 0.5f };

    // Keep process arguments separate from shell syntax and capture compiler diagnostics.
    // https://wiki.libsdl.org/SDL3/SDL_CreateProcessWithProperties
    static auto RunNativeBuild( const char* const* arguments ) -> void {
        const auto properties{ SDL_CreateProperties() };
        if ( !properties ) {
            MKT_THROW_RUNTIME_ERROR( SDL_GetError() );
        }

        SDL_SetPointerProperty( properties, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, const_cast<const char**>( arguments ) );
        SDL_SetNumberProperty( properties, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP );
        SDL_SetBooleanProperty( properties, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true );
        eastl::unique_ptr<SDL_Process, decltype( &SDL_DestroyProcess )> process{
            SDL_CreateProcessWithProperties( properties ), &SDL_DestroyProcess
        };
        SDL_DestroyProperties( properties );

        if ( !process ) {
            MKT_THROW_RUNTIME_ERROR( SDL_GetError() );
        }

        int exitCode{};
        core::usize size{};
        const eastl::unique_ptr<void, decltype( &SDL_free )> output{
            SDL_ReadProcess( process.get(), &size, &exitCode ), &SDL_free
        };
        if ( !output || exitCode != 0 ) {
            const auto* diagnostics{ output ? static_cast<const char*>( output.get() ) : SDL_GetError() };
            MKT_THROW_RUNTIME_ERROR( diagnostics );
        }
    }

    // Each file builds a module with one exported registration function. The
    // build directory is reused so CMake can track the source's included files.
    static constexpr core::cstr kNativeCMakeSource{
        "# Copyright 2026 Mikoto contributors\n"
        "# Licensed under the Apache License, Version 2.0.\n"
        "cmake_minimum_required(VERSION 3.20)\n"
        "project(MikotoNativeScript LANGUAGES CXX)\n"
        "add_library(MikotoNativeScript MODULE \"${MIKOTO_NATIVE_SOURCE}\")\n"
        "target_compile_features(MikotoNativeScript PRIVATE cxx_std_20)\n"
        "target_include_directories(MikotoNativeScript PRIVATE \"${MIKOTO_NATIVE_SDK}\")\n"
        "set_target_properties(MikotoNativeScript PROPERTIES PREFIX \"\" SUFFIX \".mktnative\"\n"
        "    CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES\n"
        "    RUNTIME_OUTPUT_DIRECTORY \"$<1:${CMAKE_BINARY_DIR}>\"\n"
        "    LIBRARY_OUTPUT_DIRECTORY \"$<1:${CMAKE_BINARY_DIR}>\")\n"
        "if(MSVC)\n"
        "    target_compile_options(MikotoNativeScript PRIVATE /utf-8 /EHsc)\n"
        "endif()\n"
    };

    auto NativeScriptingService::LoadScript( const filesystem::Path &path, scene::Entity *entity ) -> ScriptHandle {
        if ( !mIsInitialized || path.IsEmpty() || !path.IsExtension( { "cpp", "cc" } ) ) {
            return {};
        }

        if ( auto file{ filesystem::FileService::Get()->LoadFile( path ) } ) {
            return core::Ref<NativeScript>::New( file, entity );
        }

        return {};
    }

    auto NativeScriptingService::IsExecutionSupported() -> bool {
        return true;
    }

    auto NativeScriptingService::GetModule( const filesystem::Path& path, bool rebuild ) -> eastl::shared_ptr<NativeScriptModule> {
        if ( !mIsInitialized || path.IsEmpty() ) {
            return {};
        }

        std::scoped_lock lock{ mMutex };
        const filesystem::Path absolute{ path.ToAbsolute() };
        auto [found, inserted]{ mModules.try_emplace( absolute ) };
        if ( inserted || rebuild ) {
            BuildModule( found->first, found->second );
        }

        return found->second.mModule;
    }

    auto NativeScriptingService::BuildModule( const filesystem::Path& path, ModuleEntry& entry ) -> void {
        std::error_code error{};
        entry.mLastSourceWrite = std::filesystem::last_write_time( path.GetPathTyped<std::filesystem::path>(), error );

        try {
            const auto* executableDirectory{ SDL_GetBasePath() };
            const filesystem::Path sdk{ executableDirectory ?
                filesystem::PathBuilder{}.SetPath( executableDirectory ).SetPath( "Resources/NativeScriptSDK" ).Build() :
                filesystem::Path{ "Resources/NativeScriptSDK" }.ToAbsolute() };

            if ( !entry.mBuildDirectory.empty() && !std::filesystem::exists( entry.mBuildDirectory / "CMakeLists.txt" ) ) {
                entry.mBuildDirectory.clear();
            }

            const filesystem::Path configuredCmake{ MIKOTO_NATIVE_CMAKE_COMMAND };
            const auto* cmake{ configuredCmake.IsFile() ? configuredCmake.GetC_Str() : "cmake" };
            if ( entry.mBuildDirectory.empty() ) {
                entry.mBuildDirectory = mBuildRoot / std::to_string( ++mBuildNumber );
                std::filesystem::create_directories( entry.mBuildDirectory );
                std::ofstream project{ entry.mBuildDirectory / "CMakeLists.txt" };
                project << kNativeCMakeSource;
                project.close();
                if ( !project ) {
                    MKT_THROW_RUNTIME_ERROR( "Could not create the native script build files." );
                }
            }

            const filesystem::Path projectPath{ entry.mBuildDirectory };
            const filesystem::Path buildPath{ entry.mBuildDirectory / "build" };
            const auto sourceArg{ string::Format( "-DMIKOTO_NATIVE_SOURCE={}", path.GetPath() ) };
            const auto sdkArg{ string::Format( "-DMIKOTO_NATIVE_SDK={}", sdk.GetPath() ) };
            if ( !std::filesystem::exists( entry.mBuildDirectory / "build" / "CMakeCache.txt" ) ) {
                const char* configure[]{ cmake, "-S", projectPath.GetC_Str(), "-B", buildPath.GetC_Str(),
                    sourceArg.c_str(), sdkArg.c_str(), "-DCMAKE_BUILD_TYPE=Release", nullptr };
                RunNativeBuild( configure );
            }

            const char* build[]{ cmake, "--build", buildPath.GetC_Str(), "--config", "Release", nullptr };
            RunNativeBuild( build );

            auto candidate{ eastl::make_shared<NativeScriptModule>() };
            const auto destination{ entry.mBuildDirectory / ( "loaded-" + std::to_string( ++mBuildNumber ) + ".mktnative" ) };
            std::filesystem::copy_file( entry.mBuildDirectory / "build" / "MikotoNativeScript.mktnative", destination );
            candidate->mLoadedPath = filesystem::Path{ destination };

            // Loading a new copy keeps the previous DLL available to live instances.
            // https://wiki.libsdl.org/SDL3/SDL_LoadObject
            candidate->mLibrary = SDL_LoadObject( candidate->mLoadedPath.GetC_Str() );
            if ( !candidate->mLibrary ) {
                MKT_THROW_RUNTIME_ERROR( SDL_GetError() );
            }

            const auto getModule{ reinterpret_cast<native::GetScriptModule>( SDL_LoadFunction( candidate->mLibrary, native::kModuleEntryPoint ) ) };
            candidate->mApi = getModule ? getModule( native::kApiVersion ) : nullptr;
            if ( !candidate->mApi || !candidate->mApi->mName || !candidate->mApi->mCreate || !candidate->mApi->mDestroy ||
                 !candidate->mApi->mStart || !candidate->mApi->mUpdate || !candidate->mApi->mStop ||
                 ( candidate->mApi->mFieldCount && !candidate->mApi->mFields ) || !candidate->ReadMetadata() ) {
                MKT_THROW_RUNTIME_ERROR( "The module must export MKT_NATIVE_SCRIPT with a compatible SDK and valid fields." );
            }

            entry.mModule = eastl::move( candidate );
            MKT_CORE_LOGGER_INFO( "Loaded native script '{}'.", path.GetC_Str() );
        } catch ( const std::exception& failure ) {
            const auto diagnostic{ string::Format( "Native script build failed for '{}': {}", path.GetC_Str(), failure.what() ) };
            MKT_CORE_LOGGER_ERROR( "{}", diagnostic );
            if ( core::RuntimeConsole::HasInstance() ) {
                core::RuntimeConsole::Get()->Error( diagnostic );
            }
        }
    }

    auto NativeScriptingService::Update( core::f32 timeStep ) -> void {
        mRefreshElapsed += timeStep;
        if ( mRefreshElapsed < kSourceCheckIntervalSeconds ) {
            return;
        }

        mRefreshElapsed = 0.0f;
        std::scoped_lock lock{ mMutex };
        for ( auto& [path, entry]: mModules ) {
            std::error_code error{};
            const auto modified{ std::filesystem::last_write_time( path.GetPathTyped<std::filesystem::path>(), error ) };
            if ( !error && modified != entry.mLastSourceWrite ) {
                BuildModule( path, entry );
            }
        }
    }

    auto NativeScriptingService::CreateScript( const filesystem::Path &path ) -> bool {
        if ( !mIsInitialized || path.IsEmpty() || path.GetNormalizedExtension() != "cpp" || path.Exists() ) {
            return false;
        }

        try {
            filesystem::FileHandle file{ filesystem::FileService::Get()->CreateNewFile( path ) };

            if ( file.IsEmpty() ) {
                return false;
            }

            file->Write(
                "#include <Scripting/Native/NativeScriptApi.hh>\n\n"
                "using mikoto::scripting::native::EntityScript;\n\n"
                "class Script final : public EntityScript {\n"
                "public:\n"
                "    auto OnUpdate( float timeStep ) -> void override {\n"
                "        auto rotation{ GetRotation() };\n"
                "        rotation.y += mSpeed * timeStep;\n"
                "        SetRotation( rotation );\n"
                "    }\n\n"
                "    MKT_NATIVE_SCRIPT_FIELDS( MKT_NATIVE_SCRIPT_FIELD( Script, mSpeed ) )\n\n"
                "private:\n"
                "    float mSpeed{ 45.0f };\n"
                "};\n\n"
                "MKT_NATIVE_SCRIPT( Script )\n" );
            return true;
        } catch ( const std::exception &error ) {
            MKT_CORE_LOGGER_ERROR( "Could not create C++ script: {}", error.what() );
            return false;
        }
    }

    NativeScriptingService::~NativeScriptingService() {
        Shutdown();
    }

    auto NativeScriptingService::Initialize() -> void {
        const auto stamp{ std::chrono::steady_clock::now().time_since_epoch().count() };
        mBuildRoot = std::filesystem::temp_directory_path() / ( "MikotoNativeScripts-" + std::to_string( stamp ) );
        mIsInitialized = true;
    }

    auto NativeScriptingService::Shutdown() -> void {
        if ( !mIsInitialized ) {
            return;
        }

        std::scoped_lock lock{ mMutex };
        mModules.clear();
        std::error_code error{};
        std::filesystem::remove_all( mBuildRoot, error );
        mIsInitialized = false;
    }
}// namespace mikoto::scripting
