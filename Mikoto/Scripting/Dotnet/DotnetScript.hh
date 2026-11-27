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

#ifndef MIKOTO_SCRIPTING_DOTNET_SCRIPT_HH
#define MIKOTO_SCRIPTING_DOTNET_SCRIPT_HH

#include <Scripting/Script.hh>

namespace mikoto::scripting {
    class DotnetScriptingService;

    /** @brief Owns one managed instance; the service shares its runtime and assembly context. */
    class DotnetScript final : public IScript {
    public:
        explicit DotnetScript( filesystem::FileHandle file, scene::Entity *entity );

        ~DotnetScript() override;

        auto Start( scene::Scene &scene ) -> void override;
        auto Stop() -> void override;
        auto Update( float timeStep ) -> void override;
        auto OnPhysicsEvent( const physics::ContactEvent &event, scene::Entity &other ) -> void override;

        /** @brief Queues replacement of this scene's managed assembly contexts.
         *
         * A failed build preserves live instances. Successful reload resets managed
         * fields for all scripts in the scene; each recreates itself on Update.
         */
        auto ReloadScript() -> void override;

        MKT_NODISCARD auto GetFile() const -> filesystem::FileHandle override;
        MKT_NODISCARD auto GetEntity() const -> scene::Entity * override;
        MKT_NODISCARD auto GetLanguage() const -> ScriptLanguage override;

        MKT_NODISCARD auto GetMetaData() const -> const ScriptClassMetadata& override;

    private:
        friend class DotnetScriptingService;

        auto CreateInstance() -> void;

        // The service calls this after replacing the shared assembly context.
        auto RestartInstance() -> void;

    private:
        // File ownership is independent of the borrowed scene entity.
        filesystem::FileHandle mFile{};
        scene::Entity *mEntity{};

        ScriptClassMetadata mScriptMetadata{};

        // Borrowed only while registered; Stop clears these before service teardown.
        DotnetScriptingService *mService{};
        scene::Scene *mScene{};
        core::u64 mSessionID{};
        core::u64 mEntityID{};
        eastl::string mClassName{};
        eastl::string mAppliedProperties{};

        bool mCreated{};
        bool mFailed{};
    };
}// namespace mikoto::scripting

#endif// MIKOTO_SCRIPTING_DOTNET_SCRIPT_HH
