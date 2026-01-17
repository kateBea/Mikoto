//
// Created by kate on 1/17/26.
//

#include <string_view>

#include <Core/RuntimeConsole.hh>
#include <Scripting/SystemBindings.hh>

namespace Mikoto {

    auto SystemBinding::Init( sol::state &state ) -> void {
        sol::table system{ state.create_named_table( "System" ) };
        sol::table console{ state.create_named_table( "Console" ) };

        constexpr bool isReadOnly{ true };
        const std::initializer_list<std::pair<sol::string_view, ConsoleLogLevel>> keys{
            { "Debug", ConsoleLogLevel::CONSOLE_DEBUG },
            { "Error", ConsoleLogLevel::CONSOLE_ERROR },
            { "Info", ConsoleLogLevel::CONSOLE_INFO },
            { "Warning", ConsoleLogLevel::CONSOLE_WARNING },
        };
        state.new_enum<ConsoleLogLevel, isReadOnly>( "LogLevel", keys );

        console.set_function( "Debug", []( const std::string_view message ) -> void { RuntimeConsole::Get()->Debug( message ); } );
        console.set_function( "Error", []( const std::string_view message ) -> void { RuntimeConsole::Get()->Error( message ); } );
        console.set_function( "Info", []( const std::string_view message ) -> void { RuntimeConsole::Get()->Info( message ); } );
        console.set_function( "Warning", []( const std::string_view message ) -> void { RuntimeConsole::Get()->Warning( message ); } );
        console.set_function( "Log", []( const ConsoleLogLevel level, const std::string_view message ) -> void { RuntimeConsole::Get()->AddLog( level, message ); } );
    }
}// namespace Mikoto
