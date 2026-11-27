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

#ifndef MIKOTO_SCRIPTING_DOTNET_SCRIPTING_SERVICE_HH
#define MIKOTO_SCRIPTING_DOTNET_SCRIPTING_SERVICE_HH

#include <EASTL/string.h>
#include <EASTL/vector.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>

#include <Scene/Scene.hh>

#include <Scripting/Script.hh>
#include <Scripting/Dotnet/DotnetInterop.hh>
#include <Scripting/Dotnet/DotnetRuntimeState.hh>

namespace mikoto::scripting {

    // https://learn.microsoft.com/en-us/dotnet/standard/garbage-collection/fundamentals

    class DotnetScript;

    /**
     * @brief Owns the shared .NET runtime, source builds and scene assembly contexts.
     * @details CoreCLR starts lazily on compilation or Play. Loading an asset does not execute
     * user code or require a .NET installation. Stop releases a scene's collectible
     * context; Shutdown releases all sessions, not the process-wide CoreCLR host.
     * Use a separate initialized service for background compilation. Calls on
     * one instance must be serialized; gameplay calls run on the engine thread.
     */
    class DotnetScriptingService final : public core::IService {
    public:
        DotnetScriptingService();

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        /** @brief Creates a C# script template without overwriting an existing file.
         *
         * @param path Destination .cs file with a valid class-name stem.
         * @return True when the initialized service writes the script successfully.
         */
        MKT_NODISCARD auto CreateScript( const filesystem::Path& path ) -> bool;

        /** @brief Loads a script and its property metadata without starting lifecycle callbacks.
         *
         * @param path C# source or managed assembly path.
         * @param entity Entity associated with the script asset.
         * @return Script handle, or an empty handle on failure.
         */
        MKT_NODISCARD auto LoadScript( const filesystem::Path& path, scene::Entity* entity ) -> ScriptHandle;

        /** @brief Compiles a source file or project without creating scene instances.
         *
         * @param source C# source or project path.
         * @return True on successful compilation; false on initialization or build failure.
         */
        MKT_NODISCARD auto Compile( const filesystem::Path& source ) -> bool;

        // Successful explicit/background builds across compiler instances. Metadata
        // inspection does not advance this revision, avoiding refresh/rebuild loops.
        MKT_NODISCARD static auto GetCompilationRevision() -> core::u64;

        /** @brief Checks build-time availability of managed scripting.
         *
         * @return True when .NET support was compiled in; does not probe installed runtimes.
         */
        MKT_NODISCARD static auto IsSupported() -> bool;

        /** @brief Invokes destruction callbacks and releases a scene's managed context.
         *
         * @param scene Live scene to stop, before its native entities are destroyed.
         */
        auto Stop( scene::Scene& scene ) -> void;

        /** @brief Reflects a script for the editor without running its lifecycle callbacks.
         * @param path Source or assembly path. @param className Optional qualified class name.
         * @param output Owned copy of reflected metadata and constructor defaults.
         * @return True if compilation, loading and reflection succeed.
         */
        auto Inspect( const filesystem::Path& path, eastl::string_view className, ScriptClassMetadata& output ) -> bool;

        ~DotnetScriptingService() override;

    private:
        friend class DotnetScript;

        // Registration borrows scripts. Each script unregisters on Stop or destruction.
        MKT_NODISCARD auto RegisterScript( scene::Scene& scene, DotnetScript& script ) -> core::u64;

        auto UnregisterScript( scene::Scene& scene, DotnetScript& script ) -> void;

        // Establish the native callback scope for each individual managed invocation.
        MKT_NODISCARD auto Execute( scene::Scene& scene, ManagedCommand command ) -> bool;

        auto RequestReload( scene::Scene& scene ) -> void;
        auto ProcessReload( scene::Scene& scene ) -> void;

    private:
        struct Session {
            core::u64 mID{};
            bool mReloadPending{};
            eastl::vector<DotnetScript*> mScripts{};
        };

        DotnetRuntimeState mRuntime{};
        ankerl::unordered_dense::map<scene::Scene*, Session> mSessions{};
    };

    struct CompilationRecord {
        core::u32 mStatus{};
        eastl::string mSource{};
        eastl::string mOutput{};
        eastl::string mDiagnostics{};
        eastl::string mRuntime{};
        core::f64 mMilliseconds{};
        core::u32 mFiles{};
        core::u32 mErrors{};
        core::u32 mWarnings{};
        core::u32 mStage{};
        core::u64 mSequence{};
    };

    // The script scheduler reports from a worker; panels consume owned snapshots.
    // No managed string or scene pointer survives the native callback.
    class DotnetCompilerStatus final {
    public:
        static auto Clear() -> void;
        static auto Report( const ManagedCompilationReport& report ) -> void;

        MKT_NODISCARD static auto GetSnapshot() -> eastl::vector<CompilationRecord>;
    };
}

#endif// MIKOTO_SCRIPTING_DOTNET_SCRIPTING_SERVICE_HH
