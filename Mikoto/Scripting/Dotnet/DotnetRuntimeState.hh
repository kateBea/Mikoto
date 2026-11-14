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

#ifndef MIKOTO_SCRIPTING_DOTNET_RUNTIME_STATE_HH
#define MIKOTO_SCRIPTING_DOTNET_RUNTIME_STATE_HH

#include <Core/Service.hh>
#include <Core/Registry.hh>

#include <Scripting/Script.hh>
#include <Scripting/Dotnet/DotnetInterop.hh>
#include <Scripting/Dotnet/DotnetBindings.hh>

namespace mikoto::scripting {

    /** @brief Owns native bindings and access to the process-wide managed runtime.
     * @details Owned by DotnetScriptingService. Calls on one instance must be serialized.
     * Scene sessions remain with the scripting service and must stop before Shutdown.
     */
    class DotnetRuntimeState final : public core::IService {
    public:
        /** @brief Registers native bindings without starting CoreCLR. */
        auto Initialize() -> void override;

        /** @brief Releases local bindings and entry-point state. CoreCLR remains loaded. */
        auto Shutdown() -> void override;

        /** @brief Starts CoreCLR lazily and binds the managed entry point.
         * @return True when managed commands can be invoked.
         */
        MKT_NODISCARD auto Start() -> bool;

        /** @brief Reports whether the managed entry point is available.
         * @return True after a successful Start; does not start the runtime.
         */
        MKT_NODISCARD auto IsStarted() const -> bool;

        /** @brief Invokes the managed bridge synchronously without starting CoreCLR.
         * @param command Command and borrowed arguments valid for the duration of the call.
         * @return True on managed success; false when unavailable or execution fails.
         */
        MKT_NODISCARD auto Call( ManagedCommand command ) -> bool;

        /** @brief Copies registered metadata; output owns all strings independently of the assembly. */
        auto GetMetadata( core::u32 classID, ScriptClassMetadata& output ) const -> bool;

    private:
        static auto RegisterScriptClass( const ManagedScriptClassInfo* info ) -> void;
        static auto RegisterScriptField( const ManagedScriptFieldInfo* info ) -> void;
        static auto UnregisterScriptClass( core::u32 classID ) -> void;
        using EntryPoint = core::i32 ( * )( const ManagedCommand* );

        auto InitBindings() -> void;

    private:
        core::Registry<DotnetBinding> mBindings{};
        ManagedNativeApi mNativeApi{};
        ankerl::unordered_dense::map<core::u32, ScriptClassMetadata> mScriptClasses{};
        EntryPoint mExecute{};
        bool mAttempted{};
    };
}// namespace mikoto::scripting

#endif// MIKOTO_SCRIPTING_DOTNET_RUNTIME_STATE_HH
