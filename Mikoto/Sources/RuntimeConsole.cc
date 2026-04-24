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

#include <string>
#include <sstream>

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/utility.h>
#include <EASTL/string_view.h>
#include <EASTL/functional.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Profiler.hh>
#include <Core/ExecuteProcess.hh>
#include <Core/RuntimeConsole.hh>

#include <Threading/TaskService.hh>

namespace mikoto::core {

    RuntimeConsole::RuntimeConsole( const RuntimeConsoleCreateInfo&  )
    {}

    auto RuntimeConsole::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        AddLog( { ConsoleLogLevel::eInfo, "RuntimeConsole initialized." } );

        RegisterDefaultCommands();

        mIsInitialized = true;
    }

    auto RuntimeConsole::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!mIsInitialized) {
            return;
        }

        MKT_CORE_LOGGER_DEBUG( "Shutting down RuntimeConsole" );

        AddLog( { ConsoleLogLevel::eInfo, "RuntimeConsole Shutting down." } );

        mRegisteredCommands.clear();
        mLogEntries.clear();
        mIsInitialized = false;
    }

    auto RuntimeConsole::RegisterCommand(
            const eastl::string& name,
            const eastl::string& desc,
            eastl::function<void( const eastl::vector<eastl::string>& )> callback ) -> void {
        std::lock_guard lock{ mCommandRegisterMutex };
        mRegisteredCommands[name] = { name, desc, eastl::move( callback ) };
    }

    auto RuntimeConsole::ExecuteCommand( const eastl::string& input ) -> void {
        // TODO: Figure out a better way to do this
        // The way to run an external command is /command_name (No space!)

        eastl::string command{ input };
        if ( string::StartsWith(input, "/" ) ) {
            command = string::Format( "/ {}", input.substr( input.find( '/' ) + 1, input.length() ) );
        }

        std::istringstream iss{ command.c_str() };

        // First word or group of letters is the command
        std::string stdCmd{};
        iss >> stdCmd;

        eastl::string cmd{ stdCmd.c_str() };

        eastl::vector<eastl::string> args{};

        std::string arg{};
        while ( iss >> arg )
            args.push_back( arg.c_str() );

        // Find the command in the list of commands and run it
        // with the set of parameters
        auto it{ mRegisteredCommands.find( cmd ) };
        if ( it != mRegisteredCommands.end() ) {
            it->second.mCallback( args );
        } else {
            AddLog( { ConsoleLogLevel::eWarning, string::Format( "Unknown command: {}", cmd ) } );
        }
    }

    auto RuntimeConsole::Error( eastl::string_view message ) -> void {
        AddLog( ConsoleLogLevel::eError, message );
    }

    auto RuntimeConsole::Info( eastl::string_view message ) -> void {
        AddLog( ConsoleLogLevel::eInfo, message );
    }

    auto RuntimeConsole::Debug( eastl::string_view message ) -> void {
        AddLog( ConsoleLogLevel::eDebug, message );
    }

    auto RuntimeConsole::Warning( eastl::string_view message ) -> void {
        AddLog( ConsoleLogLevel::eWarning, message );
    }

    auto RuntimeConsole::AddLog( ConsoleMessage message ) -> void {
        eastl::string prefix{};

        switch ( message.Level ) {
            case ConsoleLogLevel::eError:
                prefix = "[ERROR] ";
                break;
            case ConsoleLogLevel::eWarning:
                prefix = "[WARN] ";
                break;
            case ConsoleLogLevel::eInfo:
                prefix = "[INFO] ";
                break;
            case ConsoleLogLevel::eDebug:
                prefix = "[DEBUG] ";
                break;
        }

        mLogEntries.emplace_back( prefix.append( message.mMessage ) );
    }

    auto RuntimeConsole::AddLog( const ConsoleLogLevel level, const eastl::string_view message ) -> void {
        AddLog( ConsoleMessage{ level, message.data() } );
    }

    auto RuntimeConsole::GetLogs() const -> const eastl::vector<eastl::string>& {
        return mLogEntries;
    }

    auto RuntimeConsole::RegisterDefaultCommands() -> void {
        RegisterCommand("echo", "Prints a message to the console",
            []( const eastl::vector<eastl::string>& args ) {
                eastl::string msg{};
                for ( auto& arg: args ) {
                    msg += string::Format( "{} ", arg);
                }

                Get()->AddLog( { ConsoleLogLevel::eInfo, msg } );
            } );

        RegisterCommand( "/", "Executes an external system command", []( const eastl::vector<eastl::string>& args ) {
            if ( args.empty() ) {
                Get()->AddLog( { ConsoleLogLevel::eWarning, "Usage: /<command> [ARGS]" } );
                return;
            }

            // Pack arguments for the command
            eastl::string cmd{};
            for ( const auto& arg: args ) {
                cmd += arg + " ";
            }

            // Submit for execution
            Get()->AddLog( { ConsoleLogLevel::eDebug, "Running command: " + cmd } );
            process::RunAsync( cmd, []( const eastl::string& line ) {
                Get()->AddLog( { ConsoleLogLevel::eInfo, line } );
            } );
        } );
    }
}// namespace Mikoto