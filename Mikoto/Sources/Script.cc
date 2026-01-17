//
// Created by kate on 1/17/26.
//

#include <exception>
#include <memory>

#include <Core/Exception.hh>
#include <Scripting/Script.hh>
#include <Scripting/LuaHelpers.hh>

namespace Mikoto {

    Script::Script( const File *file, sol::state &state, Entity* entity )
        : m_Entity{ entity }, m_State{ std::addressof( state ) }, m_File{ file }
    {
        // Create an isolated environment for THIS script
        sol::environment env{ state, sol::create, state.globals() };

        // Inject global Entity before executing script so it is available
        env["Entity"] = m_Entity;

        // Execute the script inside this environment
        sol::protected_function_result result{
            state.safe_script(m_File->GetFileContents(), env, sol::script_default_on_error)
        };

        if (!result.valid()) {
            const sol::error err { result };
            MKT_THROW_RUNTIME_ERROR(fmt::format("Lua error in {}: {}", m_File->GetPath(), err.what()));
        }

        // Store the environment as the object table
        m_Object = env;

        // Extract the callbacks from the environment
        m_OnCreate = env["OnCreate"];
        m_OnUpdate = env["OnUpdate"];

        // Call inside ctor resolved at compile time
        Script::Initialize();
    }

    auto Script::OnCreate() -> void {
        MKT_SOL_CALL(m_OnCreate);
    }

    auto Script::OnUpdate(float dt) -> void {
        MKT_SOL_CALL(m_OnUpdate, dt);
    }

    auto Script::Update( float dt ) -> void {
        // called every frame when scene is simulating
        if (!m_Enabled) {
            return;
        }

        OnUpdate( dt );
    }

    auto Script::SetEnable( const bool value ) -> void {
        m_Enabled = value;
    }

    auto Script::IsEnabled() const -> bool {
        return  m_Enabled;
    }

    auto Script::GetFile() const -> const File * {
        return m_File;
    }

    Script::~Script() {
        if (m_IsAllocated) {
            Release();
        }
    }

    auto Script::Initialize() -> void {
        m_OnCreate = m_Object["OnCreate"];
        m_OnUpdate = m_Object["OnUpdate"];

        OnCreate();

        m_IsAllocated = true;
    }

    auto Script::Release() -> void {
        m_IsAllocated = false;
    }
}// namespace Mikoto
