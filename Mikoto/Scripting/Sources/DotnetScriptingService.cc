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

#include <mutex>
#include <cstddef>
#include <exception>

#include <EASTL/atomic.h>
#include <EASTL/vector.h>
#include <EASTL/algorithm.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Profiler.hh>
#include <Core/RuntimeConsole.hh>

#include <Logging/Logger.hh>

#include <Filesystem/FileService.hh>

#include <Scene/Scene.hh>

#include <Scripting/Dotnet/DotnetScript.hh>
#include <Scripting/Dotnet/DotnetBindings.hh>
#include <Scripting/Dotnet/DotnetScriptingService.hh>

// Hosting flow: https://learn.microsoft.com/en-us/dotnet/core/tutorials/netcore-hosting
namespace mikoto::scripting {
    using namespace core;
    using namespace filesystem;


    // IDs must remain unique across independent gameplay and compiler services.
    static eastl::atomic<u64> sNextSession{ 1 };
    static eastl::atomic<u64> sCompilationRevision{};


    static std::mutex sCompilationStatusMutex{};

    static eastl::vector<CompilationRecord> sCompilationRecords{};

    static core::u64 sCompilationSequence{};


    DotnetScriptingService::DotnetScriptingService() {
    }

    auto DotnetScriptingService::Initialize() -> void {
        MKT_BEGIN_PROFILER();

        if ( mIsInitialized ) {
            return;
        }

        mRuntime.Initialize();
        mIsInitialized = true;
    }

    auto DotnetScriptingService::Compile( const filesystem::Path& source ) -> bool {
        if ( !mIsInitialized || !mRuntime.Start() ) {
            ManagedCompilationReport report{
                .mSize = sizeof( ManagedCompilationReport ),
                .mStatus = 3,
                .mSource = source.GetC_Str(),
                .mDiagnostics = "Could not initialize the .NET compiler. See Console for setup details.",
                .mErrors = 1
            };
            DotnetCompilerStatus::Report( report );
            return false;
        }

        if ( !mRuntime.Call( { .mOperation = ManagedOperation::eCompile, .mAssembly = source.GetC_Str() } ) ) {
            return false;
        }

        sCompilationRevision.fetch_add( 1, eastl::memory_order_release );
        return true;
    }

    auto DotnetScriptingService::GetCompilationRevision() -> core::u64 {
        return sCompilationRevision.load( eastl::memory_order_acquire );
    }

    auto DotnetScriptingService::Inspect( const filesystem::Path& path, eastl::string_view className, ScriptClassMetadata& output ) -> bool {
        if ( !mIsInitialized || !mRuntime.Start() ) {
            return false;
        }

        const auto session{ sNextSession.fetch_add( 1, eastl::memory_order_relaxed ) };
        const eastl::string name{ className };
        u32 classID{};
        const bool inspected{ mRuntime.Call( { .mOperation = ManagedOperation::eInspect, .mScene = session, .mAssembly = path.GetC_Str(), .mClass = name.c_str(), .mClassID = &classID } ) };
        const bool copied{ inspected && mRuntime.GetMetadata( classID, output ) };
        ( void )mRuntime.Call( { .mOperation = ManagedOperation::eStopScene, .mScene = session } );
        return copied;
    }

    auto DotnetScriptingService::IsSupported() -> bool {
#if MIKOTO_ENABLE_DOTNET_SCRIPTING
        return true;
#else
        return false;
#endif
    }

    DotnetScriptingService::~DotnetScriptingService() {
        if ( mIsInitialized ) {
            Shutdown();
        }
    }

    auto DotnetScriptingService::RegisterScript( scene::Scene& scene, DotnetScript& script ) -> u64 {
        if ( !mIsInitialized ) {
            return 0;
        }

        auto [it, inserted]{ mSessions.try_emplace( &scene ) };
        auto& session{ it->second };

        if ( inserted ) {
            session.mID = sNextSession.fetch_add( 1, eastl::memory_order_relaxed );
        }

        session.mScripts.push_back( &script );
        return session.mID;
    }

    auto DotnetScriptingService::UnregisterScript( scene::Scene& scene, DotnetScript& script ) -> void {
        const auto it{ mSessions.find( &scene ) };

        if ( it != mSessions.end() ) {
            auto& scripts{ it->second.mScripts };
            scripts.erase( eastl::remove( scripts.begin(), scripts.end(), &script ), scripts.end() );
        }
    }

    auto DotnetScriptingService::Execute( scene::Scene& scene, ManagedCommand command ) -> bool {
        if ( !mIsInitialized ) {
            return false;
        }

        // Stopping a never-executed script must not launch CoreCLR just for cleanup.
        if ( command.mOperation == ManagedOperation::eDestroy && !mRuntime.IsStarted() ) {
            return true;
        }

        if ( !mRuntime.Start() ) {
            return false;
        }

        const auto it{ mSessions.find( &scene ) };

        if ( it == mSessions.end() || it->second.mID != command.mScene ) {
            return false;
        }

        DotnetSceneBinding::CallbackScope scope{ scene, command.mScene };
        return mRuntime.Call( command );
    }

    auto DotnetScriptingService::RequestReload( scene::Scene& scene ) -> void {
        const auto it{ mSessions.find( &scene ) };

        if ( it != mSessions.end() ) {
            it->second.mReloadPending = true;
        }
    }

    auto DotnetScriptingService::ProcessReload( scene::Scene& scene ) -> void {
        const auto it{ mSessions.find( &scene ) };

        if ( it == mSessions.end() || !it->second.mReloadPending ) {
            return;
        }

        it->second.mReloadPending = false;

        // The managed side stages new assemblies first. Build failure preserves live objects.
        if ( !Execute( scene, { .mOperation = ManagedOperation::eReloadScene, .mScene = it->second.mID } ) ) {
            return;
        }

        for ( auto* script: it->second.mScripts ) {
            script->RestartInstance();
        }
    }

    auto DotnetScriptingService::Stop( scene::Scene& scene ) -> void {
        if ( !mIsInitialized ) {
            return;
        }

        const auto it{ mSessions.find( &scene ) };

        if ( it == mSessions.end() ) {
            return;
        }

        // Stop detaches even handles retained outside the scene, before entities disappear.
        while ( !it->second.mScripts.empty() ) {
            it->second.mScripts.back()->Stop();
        }

        DotnetSceneBinding::CallbackScope scope{ scene, it->second.mID };
        ( void )mRuntime.Call( { .mOperation = ManagedOperation::eStopScene, .mScene = it->second.mID } );
        mSessions.erase( it );
    }

    auto DotnetScriptingService::Shutdown() -> void {
        if ( !mIsInitialized ) {
            return;
        }

        // Scene teardown normally removes all sessions; detach any remaining live scripts too.
        while ( !mSessions.empty() ) {
            Stop( *mSessions.begin()->first );
        }

        mRuntime.Shutdown();
        mIsInitialized = false;
    }

    auto DotnetScriptingService::LoadScript( const filesystem::Path& path, scene::Entity* entity ) -> ScriptHandle {
        if ( !mIsInitialized || path.IsEmpty() ) {
            return {};
        }

        const auto extension{ path.GetNormalizedExtension() };

        if ( extension != "cs" && extension != "dll" ) {
            MKT_CORE_LOGGER_ERROR( "C#: choose a .cs source file or managed .dll assembly." );
            return {};
        }

        if ( auto file{ filesystem::FileService::Get()->LoadFile( path ) } ) {
            auto script{ core::Ref<DotnetScript>::New( file, entity ) };
            eastl::string className{};

            if ( entity && entity->HasComponent<scene::ScriptComponent>() ) {
                const auto& component{ entity->GetComponent<scene::ScriptComponent>() };

                if ( component.GetFilePath().GetAbsolute() == path.GetAbsolute() ) {
                    className = component.GetClassName();
                }
            }
            // Properties must be available on assignment and scene load, even
            // before Play. Inspect uses the compiled assembly, not another build.
            ( void )Inspect( path, className, script->mScriptMetadata );
            return script;
        }

        return {};
    }

    auto DotnetScriptingService::CreateScript( const filesystem::Path& path ) -> bool {
        MKT_BEGIN_PROFILER();

        if ( !mIsInitialized ) {
            return false;
        }

        try {
            if ( path.IsEmpty() || !path.IsExtension( "cs" ) || path.Exists() ) {
                MKT_CORE_LOGGER_ERROR( "Choose a new .cs file; existing scripts are not overwritten." );
                return false;
            }

            // Run validations
            const auto name{ path.GetStem() };
            const auto isLetter{ []( char character ) {
                return ( character >= 'a' && character <= 'z' ) ||
                       ( character >= 'A' && character <= 'Z' ) || character == '_';
            } };

            if ( name.empty() || !isLetter( name.front() ) ) {
                MKT_CORE_LOGGER_ERROR( "Start the C# script filename with a letter or underscore, e.g. Player.cs." );
                return false;
            }

            for ( const char character: name ) {
                if ( !isLetter( character ) && !( character >= '0' && character <= '9' ) ) {
                    MKT_CORE_LOGGER_ERROR( "Use letters, digits and underscores for a new script filename, e.g. PlayerController.cs." );
                    return false;
                }
            }

            // Escape the identifier so even a keyword filename is valid C#.
            // The reflected class name still matches the file's stem.
            const eastl::string source{ string::Format(
                "using Mikoto;\n\npublic sealed class @{} : EntityScript\n{{\n"
                "    public override void OnCreate()\n    {{\n    }}\n\n"
                "    public override void OnUpdate(float timeStep)\n    {{\n"
                "        // Example: Rotation += new Vector3(0, 45 * timeStep, 0);\n    }}\n\n"
                "    public override void OnDestroy()\n    {{\n    }}\n}}\n",
                name ) };

            filesystem::FileHandle file{ filesystem::FileService::Get()->CreateNewFile( path ) };

            if ( file.IsEmpty() ) {
                return false;
            }

            file->Write( source );
            return true;
        } catch ( const std::exception& error ) {
            MKT_CORE_LOGGER_ERROR( "Could not create C# script: {}", error.what() );
            return false;
        }
    }


    auto DotnetCompilerStatus::Report( const ManagedCompilationReport& report ) -> void {
        if ( report.mSize != sizeof( ManagedCompilationReport ) ) {
            return;
        }

        CompilationRecord record{
            .mStatus = report.mStatus,
            .mSource = report.mSource ? report.mSource : "",
            .mOutput = report.mOutput ? report.mOutput : "",
            .mDiagnostics = report.mDiagnostics ? report.mDiagnostics : "",
            .mRuntime = report.mRuntime ? report.mRuntime : "",
            .mMilliseconds = report.mMilliseconds,
            .mFiles = report.mFiles,
            .mErrors = report.mErrors,
            .mWarnings = report.mWarnings,
            .mStage = report.mReserved
        };

        if ( auto* console{ core::RuntimeConsole::GetPtr() }; console && report.mStatus != 0 && !record.mDiagnostics.empty() ) {
            const eastl::string message{ eastl::string{ "C#: " } + record.mSource + "\n" + record.mDiagnostics };

            if ( report.mStatus == 3 ) {
                console->Error( message );
            }
            else if ( report.mWarnings ) {
                console->Warning( message );
            }
            else {
                console->Info( message );
            }
        }

        if ( record.mDiagnostics.size() > 65536 ) {
            record.mDiagnostics.resize( 65536 );
            record.mDiagnostics += "\n[Further diagnostics omitted; see Console.]";
        }

        std::lock_guard lock{ sCompilationStatusMutex };
        record.mSequence = ++sCompilationSequence;

        if ( !sCompilationRecords.empty() && sCompilationRecords.back().mStatus == 0 && sCompilationRecords.back().mSource == record.mSource ) {
            sCompilationRecords.back() = eastl::move( record );
        }
        else {
            if ( sCompilationRecords.size() == 64 ) {
                sCompilationRecords.erase( sCompilationRecords.begin() );
            }

            sCompilationRecords.push_back( eastl::move( record ) );
        }
    }

    auto DotnetCompilerStatus::Snapshot() -> eastl::vector<CompilationRecord> {
        std::lock_guard lock{ sCompilationStatusMutex };
        return sCompilationRecords;
    }

    auto DotnetCompilerStatus::Clear() -> void {
        std::lock_guard lock{ sCompilationStatusMutex };
        sCompilationRecords.clear();
    }
}
