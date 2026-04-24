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

#include <exception>

#include <EASTL/memory.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Exception.hh>

#include <Scripting/LuaHelpers.hh>

#include <Scripting/Script.hh>

namespace mikoto::scripting {

    Script::Script( FileHandle file, sol::state &state, Entity* entity )
        : mEntity{ entity }, mState{ std::addressof( state ) }, mFile{ file }
    {
        // Create an isolated environment for THIS script
        sol::environment env{ state, sol::create, state.globals() };

        // Inject global Entity before executing script so it is available
        env["Entity"] = mEntity;

        // Execute the script inside this environment
        sol::protected_function_result result{
            state.script_file(mFile->GetPath().GetC_Str(), env, sol::script_default_on_error)
        };

        if (!result.valid()) {
            // This works because protected_function_result has an implicit conversion
            // operator to sol::error, but only for assignment, not brace init.
            const sol::error err = result;
            MKT_THROW_RUNTIME_ERROR(string::Format("Lua error in {}: {}", mFile->GetPath().GetC_Str(), err.what()));
        }

        // Store the environment as the object table
        mObject = env;

        // Call inside ctor resolved at compile time
        Script::Initialize();
    }

    auto Script::OnCreate() -> void {
        MKT_SOL_CALL(mOnCreate);
    }

    auto Script::OnUpdate(float dt) -> void {
        MKT_SOL_CALL(mOnUpdate, dt);
    }

    auto Script::Update( const float dt ) -> void {
        // called every frame when scene is simulating
        if (!mEnabled) {
            return;
        }

        OnUpdate( dt );
    }

    auto Script::ReloadScript(sol::state& state) -> void {
        try {
            // Create an isolated environment for THIS script
            sol::environment env{ state, sol::create, state.globals() };

            // Inject global Entity before executing script so it is available
            env["Entity"] = mEntity;

            // Execute the script inside this environment
            sol::protected_function_result result{
                state.script_file(mFile->GetPath().GetC_Str(), env, sol::script_default_on_error)
            };

            if (!result.valid()) {
                // This works because protected_function_result has an implicit conversion
                // operator to sol::error, but only for assignment, not brace init.
                const sol::error err = result;
                MKT_THROW_RUNTIME_ERROR(string::Format("Lua error reloading {}: {}", mFile->GetPath().GetC_Str(), err.what()));
            }

            // Store the environment as the object table
            mObject = env;

            // Call inside ctor resolved at compile time
            Initialize();
        } catch (std::exception& e) {
            MKT_CORE_LOGGER_ERROR( "Failed to reload script {} : {}", mFile->GetPath().GetC_Str(), e.what() );
        }
    }

    auto Script::SetEnable( const bool value ) -> void {
        mEnabled = value;
    }

    auto Script::IsEnabled() const -> bool {
        return  mEnabled;
    }

    auto Script::GetFile() const -> FileHandle {
        return mFile;
    }

    Script::~Script() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto Script::Initialize() -> void {
        mOnCreate = mObject["OnCreate"];
        mOnUpdate = mObject["OnUpdate"];

        OnCreate();

        mIsAllocated = true;
    }

    auto Script::Release() -> void {
        mIsAllocated = false;
    }
}// namespace Mikoto
