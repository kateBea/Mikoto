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

#ifndef MIKOTO_SCRIPTING_SCRIPTING_SERVICE_HH
#define MIKOTO_SCRIPTING_SCRIPTING_SERVICE_HH

#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>

#include <Filesystem/Path.hh>
#include <Filesystem/File.hh>

#include <Scripting/Lua/LuaScriptingService.hh>
#include <Scripting/Dotnet/DotnetScriptingService.hh>
#include <Scripting/Native/NativeScriptingService.hh>

namespace mikoto::scripting {

    struct ScriptingServiceDescription {
        filesystem::Path mScriptBasePath{};
    };

    /**
     * @brief Owns the language services and dispatches script asset operations.
     *
     * Initialize/Shutdown follow the engine service lifetime. Scene instances
     * must be stopped before Shutdown. All instance operations run on the
     * engine thread; background C# builds use a separate DotnetScriptingService.
     */
    class ScriptingService final : public core::ISubsystem, public core::Singleton<ScriptingService> {
    public:
        explicit ScriptingService( const ScriptingServiceDescription& config );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;
        auto Update( float timeStep ) -> void override;

        /** Route gameplay input to the focused game view, independently of editor navigation. */
        auto SetInputFocus( bool focused, bool pointerOver ) -> void;
        MKT_NODISCARD auto HasInputFocus() const -> bool;
        MKT_NODISCARD auto IsPointerOverInput() const -> bool;

        /** @brief Writes a new source template without instantiating it or overwriting a file. */
        MKT_NODISCARD auto CreateScript( ScriptLanguage language, const filesystem::Path& path ) -> bool;

        /** @brief Creates a uniquely named source template in its language folder.
         * @param language Language of the new script.
         * @return Created source path, or an empty path on failure.
         */
        MKT_NODISCARD auto CreateScript( ScriptLanguage language ) -> filesystem::Path;

        /** @brief Selects the current project's Scripts directory.
         * @param path Root containing Native, Lua and CSharp folders.
         */
        auto SetScriptBasePath( const filesystem::Path& path ) -> void;
        MKT_NODISCARD auto GetScriptBasePath() const -> const filesystem::Path&;
        /** @param language Language whose source directory is requested.
         * @return Language directory below the current script root.
         */
        MKT_NODISCARD auto GetScriptDirectory( ScriptLanguage language ) const -> filesystem::Path;

        /** @brief Creates a uniquely named template in the configured script directory and loads it. */
        MKT_NODISCARD auto CreateScript( ScriptLanguage language, scene::Entity* entity, bool watch = true ) -> ScriptHandle;

        /**
         * @brief Loads a script for an entity using the selected language.
         * @details Lua executes its initializer immediately; C# returns a source/assembly
         * resource, with managed instances created by the scene's Play session.
         * C++ returns a source-only resource and never compiles or executes code.
         * @param watch Enables Lua source reloads on the engine thread. Runtime copies
         * should disable this so editor watches cannot retain their entities.
         */
        MKT_NODISCARD auto LoadScript(
            ScriptLanguage language,
            const filesystem::Path& path,
            scene::Entity* entity, bool watch = true ) -> ScriptHandle;

        MKT_NODISCARD auto GetNativeScriptingService() const -> NativeScriptingService*;
        MKT_NODISCARD auto GetLuaScriptingService() const -> LuaScriptingService*;
        MKT_NODISCARD auto GetDotnetScriptingService() const -> DotnetScriptingService*;

        ~ScriptingService() override;

    private:
        filesystem::Path mBasePath{};
        bool mInputFocused{ true };
        bool mPointerOverInput{ true };

        eastl::unique_ptr<LuaScriptingService> mLuaService{};
        eastl::unique_ptr<DotnetScriptingService> mDotnetService{};
        eastl::unique_ptr<NativeScriptingService> mNativeService{};
    };
}// namespace mikoto::scripting


#endif// MIKOTO_SCRIPTING_SCRIPTING_SERVICE_HH
