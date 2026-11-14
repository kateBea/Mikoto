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

#ifndef MIKOTO_SCRIPTING_NATIVE_SCRIPTING_SERVICE_HH
#define MIKOTO_SCRIPTING_NATIVE_SCRIPTING_SERVICE_HH

#include <mutex>
#include <filesystem>

#include <EASTL/shared_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>

#include <Filesystem/Path.hh>
#include <Filesystem/File.hh>

#include <Scripting/Script.hh>

namespace mikoto::scripting {
    struct NativeScriptModule;

    /** Compiles source modules and owns their latest successful versions. */
    class NativeScriptingService final : public core::IService {
    public:
        ~NativeScriptingService() override;

        auto Initialize() -> void override;
        auto Shutdown() -> void override;
        auto Update( core::f32 timeStep ) -> void;

        MKT_NODISCARD auto CreateScript( const filesystem::Path &path ) -> bool;
        MKT_NODISCARD auto LoadScript( const filesystem::Path &path, scene::Entity *entity ) -> ScriptHandle;

        MKT_NODISCARD auto IsExecutionSupported() -> bool;
        MKT_NODISCARD auto GetModule( const filesystem::Path& path, bool rebuild = false ) -> eastl::shared_ptr<NativeScriptModule>;

    private:
        struct ModuleEntry {
            eastl::shared_ptr<NativeScriptModule> mModule{};
            std::filesystem::path mBuildDirectory{};
            std::filesystem::file_time_type mLastSourceWrite{};
        };

        auto BuildModule( const filesystem::Path& path, ModuleEntry& entry ) -> void;

        std::mutex mMutex{};
        ankerl::unordered_dense::map<filesystem::Path, ModuleEntry> mModules{};
        std::filesystem::path mBuildRoot{};
        core::u64 mBuildNumber{};
        core::f32 mRefreshElapsed{};
    };
}// namespace mikoto::scripting

#endif// MIKOTO_SCRIPTING_NATIVE_SCRIPTING_SERVICE_HH
