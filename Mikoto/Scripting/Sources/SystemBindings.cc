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


#include <initializer_list>

#include <Core/Profiler.hh>
#include <Core/RuntimeConsole.hh>

#include <Scripting/Lua/LuaBindings.hh>


namespace mikoto::scripting {

    using namespace mikoto::core;

    auto SystemBinding::Init( sol::state& state ) -> void {
        MKT_BEGIN_PROFILER();
        sol::table system{ state.create_named_table( "System" ) };
        sol::table console{ state.create_named_table( "Console" ) };

        constexpr bool isReadOnly{ true };
        const std::initializer_list<std::pair<sol::string_view, core::ConsoleLogLevel>> keys{
            { "Debug", core::ConsoleLogLevel::eDebug },
            { "Error", core::ConsoleLogLevel::eError },
            { "Info", core::ConsoleLogLevel::eInfo },
            { "Warning", core::ConsoleLogLevel::eWarning },
        };
        state.new_enum<core::ConsoleLogLevel, isReadOnly>( "LogLevel", keys );

        console.set_function( "Debug", []( const char* message ) -> void {
            core::RuntimeConsole::Get()->Debug( message );
        } );
        console.set_function( "Error", []( const char* message ) -> void {
            core::RuntimeConsole::Get()->Error( message );
        } );
        console.set_function( "Info", []( const char* message ) -> void {
            core::RuntimeConsole::Get()->Info( message );
        } );
        console.set_function( "Warning", []( const char* message ) -> void {
            core::RuntimeConsole::Get()->Warning( message );
        } );
        console.set_function( "Log", []( const core::ConsoleLogLevel level, const char* message ) -> void {
            core::RuntimeConsole::Get()->AddLog( level, message );
        } );
    }
}// namespace Mikoto
