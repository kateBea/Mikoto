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
#include <chrono>
#include <filesystem>

#include <ankerl/unordered_dense.h>

#include <Logging/Logger.hh>

#include <Threading/TaskService.hh>

#include <Filesystem/FileWatcherService.hh>

#include <Scene/Component.hh>

#include <Scripting/ScriptBuildTracker.hh>
#include <Scripting/Dotnet/DotnetScriptingService.hh>

namespace mikoto::scripting {
    using namespace core;
    using namespace filesystem;

    struct ScriptBuildTracker::State {
        using Clock = std::chrono::steady_clock;

        struct Pending {
            filesystem::Path mSource{};
            Clock::time_point mDue{};
        };
        // Watch callbacks only enqueue changes; the editor schedules the builds.
        std::mutex mMutex{};
        ankerl::unordered_dense::map<filesystem::Path, Pending> mPending{};
        bool mAcceptingChanges{ true };
        bool mBuilding{};

        // The shared watcher service owns the actual directory watchers.
        ankerl::unordered_dense::set<filesystem::Path> mWatchedDirectories{};
        Clock::time_point mScanAfter{};

        DotnetScriptingService mCompiler{};

        auto Retire() -> void {
            std::lock_guard lock{ mMutex };
            mAcceptingChanges = false;
            mPending.clear();
        }

        static auto IsSource( const std::filesystem::path& path ) -> bool {
            auto extension{ path.extension().string() };

            for ( auto& character: extension ) {
                if ( character >= 'A' && character <= 'Z' ) {
                    character += 'a' - 'A';
                }
            }

            return extension == ".cs";
        }

        auto WatchDirectory( const filesystem::Path& path, const eastl::shared_ptr<State>& self ) -> void {
            if ( path.IsEmpty() || !path.Exists() ) {
                return;
            }

            const auto directory{ path.ToAbsolute() };

            if ( mWatchedDirectories.contains( directory ) ) {
                return;
            }

            // The service shares directory watchers with other clients. Weak
            // callbacks never retain a closed project or touch its scene.
            FileWatcherService::Get()->Watch( directory, [weak = eastl::weak_ptr<State>{ self }]( const filesystem::Path& changed, filesystem::FileWatchEvent ) {
                const auto native{ changed.GetPathTyped<std::filesystem::path>() };

                if ( !IsSource( native ) ) {
                    return;
                }

                for ( const auto& part: native ) {
                    if ( part == ".cache" || part == "obj" || part == "bin" || part == ".git" ) {
                        return;
                    }
                }

                if ( auto state{ weak.lock() } ) {
                    std::lock_guard lock{ state->mMutex };

                    if ( !state->mAcceptingChanges ) {
                        return;
                    }

                    state->mPending[filesystem::Path{ native.parent_path() }] = { changed, Clock::now() + std::chrono::milliseconds{ 800 } };
                }
            } );

            mWatchedDirectories.insert( directory );
        }
    };

    ScriptBuildTracker::ScriptBuildTracker() = default;

    ScriptBuildTracker::~ScriptBuildTracker() {
        if ( mState ) {
            // A queued build can retain State after the tracker is destroyed.
            // Retire it explicitly so shared watchers cannot enqueue more work.
            mState->Retire();
        }
    }

    auto ScriptBuildTracker::IsBuilding() const -> bool {
        if ( !mState ) {
            return false;
        }

        std::lock_guard lock{ mState->mMutex };
        return mState->mBuilding || !mState->mPending.empty();
    }

    auto ScriptBuildTracker::Update( scene::Scene* scene, const filesystem::Path& assets, const filesystem::Path& project ) -> void {
        if ( !DotnetScriptingService::IsSupported() ) {
            return;
        }

        if ( !mState || assets != mAssets || project != mProject ) {
            if ( mState ) {
                mState->Retire();
            }

            mState = eastl::make_shared<State>();
            mState->mCompiler.Initialize();
            mAssets = assets;
            mProject = project;
            // Build the complete script project once when it opens, before any
            // source edit. The existing worker/debounce path handles later saves.
            if ( !project.IsEmpty() && project.Exists() ) {
                mState->mPending[project.GetDirectory()] = { project, State::Clock::now() };
            }
        }

        const auto state{ mState };
        const auto now{ State::Clock::now() };

        if ( now >= state->mScanAfter ) {
            state->mScanAfter = now + std::chrono::seconds{ 2 };

            try {
                state->WatchDirectory( assets, state );

                if ( scene ) {
                    const auto view{ scene->GetRegistry().view<scene::ScriptComponent>() };

                    for ( const auto entity: view ) {
                        const auto& script{ view.get<scene::ScriptComponent>( entity ) };
                        const auto path{ script.GetFilePath().GetPathTyped<std::filesystem::path>() };

                        if ( script.GetLanguage() == ScriptLanguage::eCSharp && State::IsSource( path ) ) {
                            state->WatchDirectory( filesystem::Path{ path.parent_path() }, state );
                        }
                    }
                }
            } catch ( const std::exception& error ) {
                MKT_CORE_LOGGER_WARN( "C# file watch: {}", error.what() );
            }
        }

        filesystem::Path source{};
        {
            std::lock_guard lock{ state->mMutex };

            if ( state->mBuilding ) {
                return;
            }

            for ( auto it{ state->mPending.begin() }; it != state->mPending.end(); ++it ) {
                if ( now >= it->second.mDue ) {
                    source = it->second.mSource;
                    state->mPending.erase( it );
                    state->mBuilding = true;
                    break;
                }
            }
        }

        if ( source.IsEmpty() ) {
            return;
        }

        try {
            threading::TaskService::Get()->ExecuteWorkerTask( [state, source]() mutable {
                try {
                    // Rename/delete events must rebuild the remaining sibling
                    // sources too; the original entry file may no longer exist.
                    if ( !source.Exists() ) {
                        const auto directory{ source.GetPathTyped<std::filesystem::path>().parent_path() };

                        for ( const auto& entry: std::filesystem::directory_iterator{ directory } ) {
                            if ( entry.is_regular_file() && State::IsSource( entry.path() ) ) {
                                source = filesystem::Path{ entry.path() };
                                break;
                            }
                        }
                    }

                    if ( source.Exists() ) {
                        ( void )state->mCompiler.Compile( source );
                    }
                } catch ( const std::exception& error ) {
                    const ManagedCompilationReport report{
                        .mSize = sizeof( ManagedCompilationReport ),
                        .mStatus = 3,
                        .mSource = source.GetC_Str(),
                        .mDiagnostics = error.what(),
                        .mErrors = 1
                    };
                    DotnetCompilerStatus::Report( report );
                }

                std::lock_guard lock{ state->mMutex };
                state->mBuilding = false;
                // Saves received during this build remain queued for the next
                // debounced build, so the last edit is never lost.
            } );
        } catch ( const std::exception& error ) {
            std::lock_guard lock{ state->mMutex };
            state->mBuilding = false;
            MKT_CORE_LOGGER_ERROR( "Could not queue C# build: {}", error.what() );
        }
    }

}// namespace mikoto::scripting
