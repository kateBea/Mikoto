//
// Created by kate on 10/28/25.
//

#include <sstream>


#include <Core/RuntimeConsole.hh>
#include <Threading/TaskService.hh>
#include "Core/ExecuteProcess.hh"

namespace Mikoto {

    RuntimeConsole::RuntimeConsole( const ConsoleManagerCreateInfo& createInfo )
        : m_Name( createInfo.Name ) {
    }

    auto RuntimeConsole::Init() -> void {
        AddLog( { ConsoleLogLevel::CONSOLE_INFO, "RuntimeConsole initialized." } );

        RegisterCommand("echo", "Prints a message to the console",
                []( const std::vector<std::string>& args ) {
                    std::string msg;
                    for ( auto& arg: args ) msg += arg + " ";
                    Get()->AddLog( { ConsoleLogLevel::CONSOLE_INFO, msg } );
                } );

        RegisterCommand( "/run_a", "Executes an external system command asynchronously", []( const std::vector<std::string>& args ) {
            if ( args.empty() ) {
                Get()->AddLog( { ConsoleLogLevel::CONSOLE_WARNING, "Usage: /mkt_ex <command>" } );
                return;
            }

            std::string cmd;
            for ( const auto& arg: args ) {
                cmd += arg + " ";
            }

            Get()->AddLog( { ConsoleLogLevel::CONSOLE_DEBUG, "Running external command (async): " + cmd } );

            ExecuteProcess::RunAsync( cmd, []( const std::string& line ) {
                // Push each line into the console output
                Get()->AddLog( { ConsoleLogLevel::CONSOLE_INFO, line } );
            } );
        } );

        RegisterCommand( "/run", "Executes an external system command", []( const std::vector<std::string>& args ) {
            if ( args.empty() ) {
                Get()->AddLog( { ConsoleLogLevel::CONSOLE_WARNING, "Usage: /mkt_ex <command>" } );
                return;
            }

            std::string cmd;
            for ( const auto& arg: args ) {
                cmd += arg + " ";
            }

            Get()->AddLog( { ConsoleLogLevel::CONSOLE_DEBUG, "Running external command: " + cmd } );

            const auto output = ExecuteProcess::Run( cmd );
            if ( output.empty() ) {
                Get()->AddLog( { ConsoleLogLevel::CONSOLE_INFO, "[no output]" } );
            } else {
                Get()->AddLog( { ConsoleLogLevel::CONSOLE_INFO, output } );
            }
        } );

        m_IsInitialized = true;
    }

    auto RuntimeConsole::Shutdown() -> void {
        AddLog( { ConsoleLogLevel::CONSOLE_INFO, "RuntimeConsole shutting down." } );
        m_Commands.clear();
        m_LogEntries.clear();
        m_IsInitialized = false;
    }

    auto RuntimeConsole::RegisterCommand(
            const std::string& name,
            const std::string& desc,
            std::function<void( const std::vector<std::string>& )> callback ) -> void {
        m_Commands[name] = { name, desc, std::move( callback ) };
    }

    auto RuntimeConsole::ExecuteCommand( const std::string& input ) -> void {
        std::istringstream iss( input );
        std::string cmd;
        iss >> cmd;

        std::vector<std::string> args;
        std::string arg;
        while ( iss >> arg )
            args.push_back( arg );

        auto it = m_Commands.find( cmd );
        if ( it != m_Commands.end() ) {
            it->second.Callback( args );
        } else {
            AddLog( { ConsoleLogLevel::CONSOLE_WARNING, "Unknown command: " + cmd } );
        }
    }

    auto RuntimeConsole::Error( std::string_view message ) -> void {
        AddLog( ConsoleLogLevel::CONSOLE_ERROR, message );
    }

    auto RuntimeConsole::Info( std::string_view message ) -> void {
        AddLog( ConsoleLogLevel::CONSOLE_INFO, message );
    }

    auto RuntimeConsole::Debug( std::string_view message ) -> void {
        AddLog( ConsoleLogLevel::CONSOLE_DEBUG, message );
    }

    auto RuntimeConsole::Warning( std::string_view message ) -> void {
        AddLog( ConsoleLogLevel::CONSOLE_WARNING, message );
    }

    auto RuntimeConsole::AddLog( ConsoleMessage message ) -> void {
        std::string prefix;
        switch ( message.Level ) {
            case ConsoleLogLevel::CONSOLE_ERROR:
                prefix = "[ERROR] ";
                break;
            case ConsoleLogLevel::CONSOLE_WARNING:
                prefix = "[WARN] ";
                break;
            case ConsoleLogLevel::CONSOLE_INFO:
                prefix = "[INFO] ";
                break;
            case ConsoleLogLevel::CONSOLE_DEBUG:
                prefix = "[DEBUG] ";
                break;
        }
        m_LogEntries.emplace_back( prefix + message.Message );
    }

    auto RuntimeConsole::AddLog( const ConsoleLogLevel level, const std::string_view message ) -> void {
        AddLog( ConsoleMessage{ level, message.data() } );
    }

}// namespace Mikoto