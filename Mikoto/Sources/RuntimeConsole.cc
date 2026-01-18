//
// Created by kate on 10/28/25.
//
#include <sstream>

#include <fmt/format.h>

#include <Core/Profiler.hh>
#include <Core/RuntimeConsole.hh>
#include <Threading/TaskService.hh>

#include "Core/ExecuteProcess.hh"

namespace Mikoto {

    RuntimeConsole::RuntimeConsole( const ConsoleManagerCreateInfo& createInfo )
        : m_Name( createInfo.Name ) {
    }

    auto RuntimeConsole::Init() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        AddLog( { ConsoleLogLevel::CONSOLE_INFO, "RuntimeConsole initialized." } );

        RegisterCommand("echo", "Prints a message to the console",
                []( const std::vector<std::string>& args ) {
                    std::string msg;
                    for ( auto& arg: args ) msg += arg + " ";
                    Get()->AddLog( { ConsoleLogLevel::CONSOLE_INFO, msg } );
                } );

        RegisterCommand( "/", "Executes an external system command", []( const std::vector<std::string>& args ) {
            if ( args.empty() ) {
                Get()->AddLog( { ConsoleLogLevel::CONSOLE_WARNING, "Usage: /<command> [ARGS]" } );
                return;
            }

            std::string cmd;
            for ( const auto& arg: args ) {
                cmd += arg + " ";
            }

            Get()->AddLog( { ConsoleLogLevel::CONSOLE_DEBUG, "Running command: " + cmd } );

            ExecuteProcess::RunAsync( cmd, []( const std::string& line ) {
                // Push each line into the console output
                Get()->AddLog( { ConsoleLogLevel::CONSOLE_INFO, line } );
            } );
        } );

        m_IsInitialized = true;
    }

    auto RuntimeConsole::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

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
        // TODO: Figure out a better way to do this
        // The way to run an external command is /command_name (No space!)

        std::string command{ input };

        if ( input.starts_with( "/" ) ) {
            command = fmt::format( "/ {}", input.substr( input.find( '/' ) + 1, input.length() ) );
        }

        std::string cmd{};
        std::istringstream iss{ command };

        // First word or group of letters is the command
        iss >> cmd;

        std::string arg{};
        std::vector<std::string> args{};

        while ( iss >> arg )
            args.push_back( arg );

        // Find the command in the list of commands and run it
        // with the set of parameters
        auto it{ m_Commands.find( cmd ) };
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