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
#include <atomic>
#include <thread>

#include <EASTL/algorithm.h>
#include <EASTL/unique_ptr.h>

#include <taskflow/taskflow.hpp>

#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Assets/Utility.hh>

namespace mikoto::asset {

    using namespace mikoto::core;
    using namespace mikoto::filesystem;

    static auto AvailableWorkers() -> core::u32 {
        // Avoid system load
        const auto count{ std::thread::hardware_concurrency() };
        return count > 3 ? count - 3 : 1;
    }

    struct ImportWorkers {
        std::mutex mMutex{};
        core::u32 mCount{ AvailableWorkers() };
        eastl::unique_ptr<tf::Executor> mExecutor{};
    };

    static auto Workers() -> ImportWorkers& {
        static ImportWorkers workers;
        return workers;
    }

    static thread_local bool gInsideImportJob{};


    auto SetImportWorkerCount( core::u32 count ) -> void {
        auto& workers{ Workers() };
        const std::lock_guard lock{ workers.mMutex };
        const auto resolved{ count ? eastl::min( count, AvailableWorkers() ) : AvailableWorkers() };

        if ( workers.mExecutor && workers.mCount != resolved ) {
            MKT_THROW_RUNTIME_ERROR( "Import worker count must be configured before imports start." );
        }

        workers.mCount = resolved;
    }

    auto GetImportWorkerCount() -> core::u32 {
        auto& workers{ Workers() };
        const std::lock_guard lock{ workers.mMutex };
        return workers.mCount;
    }

    auto RunImportJobs( core::usize count, const eastl::function<void( core::usize )>& job ) -> void {
        if ( !count ) {
            return;
        }

        if ( gInsideImportJob ) {
            for ( core::usize index{}; index < count; ++index ) {
                job( index );
            }

            return;
        }

        auto& workers{ Workers() };
        tf::Executor* executor{};
        {
            // Spawn workers if they do not exist yet
            const std::lock_guard lock{ workers.mMutex };

            if ( !workers.mExecutor ) {
                workers.mExecutor = eastl::make_unique<tf::Executor>( workers.mCount );
            }

            executor = workers.mExecutor.get();
        }

        tf::Taskflow tasks{};
        std::atomic<core::usize> next{};

        // Construct tasks
        for ( core::usize worker{}; worker < eastl::min( count, executor->num_workers() ); ++worker ) {
            tasks.emplace( [&] {
                struct Scope {
                    Scope() {
                        gInsideImportJob = true;
                    }

                    ~Scope() {
                        gInsideImportJob = false;
                    }
                } scope{};

                for ( ;; ) {
                    const auto index{ next.fetch_add( 1, std::memory_order_relaxed ) };

                    if ( index >= count ) {
                        break;
                    }

                    job( index );
                }
            } );
        }

        // Drain queue
        executor->run( tasks ).get();
    }


    auto IsFileImage( const filesystem::Path& path ) -> bool {
        MKT_BEGIN_PROFILER();
        const auto& ext{ path.GetNormalizedExtension() };

        return ext == "png" ||
               ext == "jpg" ||
               ext == "jpeg" ||
               ext == "tga" ||
               ext == "bmp" ||
               ext == "hdr" ||
               ext == "ktx2";
    }

    auto IsFileImage( filesystem::FileType type ) -> bool {
        MKT_BEGIN_PROFILER();
        return type == filesystem::FileType::ePng ||
               type == filesystem::FileType::eJpg ||
               type == filesystem::FileType::eJpeg ||
               type == filesystem::FileType::eBmp ||
               type == filesystem::FileType::eTarga ||
               type == filesystem::FileType::eHdr ||
               type == filesystem::FileType::eKtx2;
    }

    auto IsFileModel( const filesystem::Path& path ) -> bool {
        MKT_BEGIN_PROFILER();
        const auto& ext{ path.GetNormalizedExtension() };

        return ext == "obj" ||
               ext == "fbx" ||
               ext == "gltf" ||
               ext == "glb";
    }

    auto IsFileModel( filesystem::FileType type ) -> bool {
        MKT_BEGIN_PROFILER();
        return type == filesystem::FileType::eObj ||
               type == filesystem::FileType::eFbx ||
               type == filesystem::FileType::eGltf ||
               type == filesystem::FileType::eGlb;
    }

    auto IsFileScene( const filesystem::Path& path ) -> bool {
        MKT_BEGIN_PROFILER();
        const auto& ext{ path.GetNormalizedExtension() };

        return ext == filesystem::kMikotoSceneExtension;
    }

    auto IsFileAudio( const filesystem::Path& path ) -> bool {
        MKT_BEGIN_PROFILER();
        const auto& ext{ path.GetNormalizedExtension() };

        return ext == "wav" ||
               ext == "mp3" ||
               ext == "ogg" ||
               ext == "flac";
    }

    auto IsFileFont( const filesystem::Path& path ) -> bool {
        MKT_BEGIN_PROFILER();
        const auto& ext{ path.GetNormalizedExtension() };

        return ext == "ttf" ||
               ext == "otf";
    }

    auto IsFileMaterial( const filesystem::Path& path ) -> bool {
        MKT_BEGIN_PROFILER();
        const auto& ext{ path.GetNormalizedExtension() };

        return ext == "mat" ||
               ext == "material" ||
               ext == "json";
    }

}// namespace mikoto::asset
