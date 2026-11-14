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
#include <exception>

#include <EASTL/utility.h>
#include <EASTL/sort.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/algorithm.h>
#include <EASTL/functional.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/CVar.hh>
#include <Core/Types.hh>
#include <Core/Process.hh>
#include <Core/Profiler.hh>
#include <Core/RuntimeConsole.hh>

#include <Logging/Logger.hh>

#include <Threading/TaskService.hh>

namespace mikoto::core {

    RuntimeConsole::RuntimeConsole( const RuntimeConsoleCreateInfo& info )
        : mMaxLogEntries{ info.mMaxLogEntries ? info.mMaxLogEntries : 1 } {
    }

    auto RuntimeConsole::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        AddLog( { ConsoleLogLevel::eInfo, "RuntimeConsole initialized." } );

        RegisterDefaultCommands();

        logging::DefaultLogger::GetPtr()->SetConsoleObserver( [this]( logging::Severity severity, eastl::string_view message ) {
            ConsoleLogLevel level{ ConsoleLogLevel::eDebug };

            switch ( severity ) {
                case logging::Severity::eError:
                case logging::Severity::eCritical:
                    level = ConsoleLogLevel::eError;
                    break;
                case logging::Severity::eWarning:
                    level = ConsoleLogLevel::eWarning;
                    break;
                case logging::Severity::eInfo:
                    level = ConsoleLogLevel::eInfo;
                    break;
                default:;
            }

            AddLog( level, message );
        } );

        mIsInitialized = true;
    }

    auto RuntimeConsole::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mIsInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_DEBUG( "Shutting down RuntimeConsole" );
        logging::DefaultLogger::GetPtr()->SetConsoleObserver( {} );

        AddLog( { ConsoleLogLevel::eInfo, "RuntimeConsole Shutting down." } );

        {
            std::lock_guard lock{ mCommandRegisterMutex };
            mRegisteredCommands.clear();
        }

        ClearLogs();
        mIsInitialized = false;
    }

    auto RuntimeConsole::RegisterCommand(
        const eastl::string& name,
        const eastl::string& desc,
        eastl::function<void( const eastl::vector<eastl::string>& )> callback ) -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mCommandRegisterMutex };
        mRegisteredCommands[name] = { name, desc, eastl::move( callback ) };
    }

    auto RuntimeConsole::ExecuteCommand( const eastl::string& input ) -> void {
        MKT_BEGIN_PROFILER();
        // TODO: Figure out a better way to do this
        // The way to run an external command is /command_name (No space!)

        eastl::string command{ input };

        if ( string::StartsWith( input, "/" ) ) {
            command = string::Format( "/ {}", input.substr( input.find( '/' ) + 1, input.length() ) );
        }

        std::istringstream iss{ command.c_str() };

        // First word or group of letters is the command
        std::string stdCmd{};
        iss >> stdCmd;

        eastl::string cmd{ stdCmd.c_str() };

        eastl::vector<eastl::string> args{};

        std::string arg{};

        while ( iss >> arg ) {
            args.push_back( arg.c_str() );
        }

        if ( cmd.empty() ) {
            return;
        }
        // Copy under the registry lock, then release it before invoking user
        // code: callbacks may register commands or query command help.
        eastl::function<void( const eastl::vector<eastl::string>& )> callback{};
        {
            std::lock_guard lock{ mCommandRegisterMutex };
            const auto it{ mRegisteredCommands.find( cmd ) };

            if ( it != mRegisteredCommands.end() ) {
                callback = it->second.mCallback;
            }
        }

        if ( callback ) {
            try {
                callback( args );
            } catch ( const std::exception& error ) {
                Error( string::Format( "Command '{}' failed: {}", cmd, error.what() ) );
            } catch ( ... ) {
                Error( string::Format( "Command '{}' failed with an unknown exception.", cmd ) );
            }
        }
        else {
            AddLog( { ConsoleLogLevel::eWarning, string::Format( "Unknown command: {}", cmd ) } );
        }
    }

    auto RuntimeConsole::GetCommands() const -> eastl::vector<CommandInfo> {
        eastl::vector<CommandInfo> commands{};
        {
            std::lock_guard lock{ mCommandRegisterMutex };
            commands.reserve( mRegisteredCommands.size() );

            for ( const auto& [name, command]: mRegisteredCommands ) {
                commands.push_back( { name, command.mDescription } );
            }
        }

        eastl::sort( commands.begin(), commands.end(), []( const auto& left, const auto& right ) {
            return left.mName < right.mName;
        } );
        return commands;
    }

    auto RuntimeConsole::Error( eastl::string_view message ) -> void {
        MKT_BEGIN_PROFILER();
        AddLog( ConsoleLogLevel::eError, message );
    }

    auto RuntimeConsole::Info( eastl::string_view message ) -> void {
        MKT_BEGIN_PROFILER();
        AddLog( ConsoleLogLevel::eInfo, message );
    }

    auto RuntimeConsole::Debug( eastl::string_view message ) -> void {
        MKT_BEGIN_PROFILER();
        AddLog( ConsoleLogLevel::eDebug, message );
    }

    auto RuntimeConsole::Warning( eastl::string_view message ) -> void {
        MKT_BEGIN_PROFILER();
        AddLog( ConsoleLogLevel::eWarning, message );
    }

    auto RuntimeConsole::AddLog( ConsoleMessage message ) -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mLogRegisterMutex };

        eastl::string prefix{};

        switch ( message.mLevel ) {
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

        // Keep the ID in copied/searchable text as well as the structured entry.
        // The counter survives ClearLogs, so an ID always identifies one message.
        const auto id{ mNextLogID };
        prefix += string::Format( "[#{:06}] ", id );
        mLogEntries.push_back( { id, message.mLevel, prefix.append( message.mMessage ) } );
        ++mLogCounts[static_cast<usize>( message.mLevel )];
        ++mNextLogID;

        if ( mLogEntries.size() > mMaxLogEntries ) {
            --mLogCounts[static_cast<usize>( mLogEntries.front().mLevel )];
            mLogEntries.pop_front();
        }
    }

    auto RuntimeConsole::AddLog( const ConsoleLogLevel level, const eastl::string_view message ) -> void {
        MKT_BEGIN_PROFILER();
        AddLog( ConsoleMessage{ level, eastl::string{ message.data(), message.size() } } );
    }

    auto RuntimeConsole::GetLogs() const -> eastl::vector<eastl::string> {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mLogRegisterMutex };
        eastl::vector<eastl::string> entries{};
        entries.reserve( mLogEntries.size() );

        for ( const auto& entry: mLogEntries ) {
            entries.push_back( entry.mText );
        }

        return entries;
    }

    auto RuntimeConsole::ReadLogUpdates( u64& nextID, eastl::vector<ConsoleLogEntry>& entries ) const -> u64 {
        MKT_BEGIN_PROFILER();
        entries.clear();
        std::lock_guard lock{ mLogRegisterMutex };
        const u64 firstID{ mLogEntries.empty() ? mNextLogID : mLogEntries.front().mID };
        // Clear and eviction both advance the retained range without resetting
        // sequence IDs, so readers cannot mistake new messages for old ones.
        if ( nextID < firstID || nextID > mNextLogID ) {
            nextID = firstID;
        }

        entries.reserve( static_cast<usize>( mNextLogID - nextID ) );

        for ( usize index{ static_cast<usize>( nextID - firstID ) }; index < mLogEntries.size(); ++index ) {
            entries.push_back( mLogEntries[index] );
        }

        nextID = mNextLogID;
        return firstID;
    }

    auto RuntimeConsole::GetLogCounts() const -> eastl::array<usize, 4> {
        MKT_BEGIN_PROFILER();
        const std::lock_guard lock{ mLogRegisterMutex };
        return mLogCounts;
    }

    auto RuntimeConsole::ClearLogs() -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mLogRegisterMutex };
        mLogEntries.clear();
        mLogCounts.fill( 0 );
    }

    auto RuntimeConsole::RegisterDefaultCommands() -> void {
        MKT_BEGIN_PROFILER();

        RegisterCommand( "help", "Lists commands, or describes one: help [command]",
            [this]( const eastl::vector<eastl::string>& args ) {
            bool found{};

            for ( const auto& command: GetCommands() ) {
                if ( args.empty() || command.mName == args.front() ) {
                    Info( string::Format( "{}: {}", command.mName, command.mDescription ) );
                    found = true;
                }
            }

            if ( !found ) {
                Warning( string::Format( "No help for '{}'. Type help to list commands.", args.front() ) );
            }
        } );

        RegisterCommand( "echo", "Prints text and CVar values: echo Title: ${application.title}",
            [this]( const eastl::vector<eastl::string>& args ) {
            eastl::string msg{};

            for ( usize index{}; index < args.size(); ++index ) {
                if ( index != 0 ) {
                    msg += " ";
                }

                msg += args[index];
            }

            eastl::string expanded{};
            usize cursor{};

            while ( cursor < msg.size() ) {
                const usize opening{ msg.find( "${", cursor ) };

                if ( opening == eastl::string::npos ) {
                    expanded.append( msg.data() + cursor, msg.size() - cursor );
                    break;
                }

                expanded.append( msg.data() + cursor, opening - cursor );

                const usize closing{ msg.find( '}', opening + 2 ) };

                if ( closing == eastl::string::npos ) {
                    Warning( "Unclosed CVar reference. Usage: echo ${name}" );
                    return;
                }

                const eastl::string_view name{ msg.data() + opening + 2, closing - opening - 2 };

                if ( name.empty() ) {
                    Warning( "Empty CVar reference. Usage: echo ${name}" );
                    return;
                }

                if ( !CVarRegistry::Get().Contains( name ) ) {
                    Warning( string::Format( "Unknown CVar '{}'.", name ) );
                    return;
                }

                // Scan only the original message. A CVar's value is literal
                // text, even when it contains another ${name} or a command.
                expanded += CVarRegistry::Get().GetString( name );
                cursor = closing + 1;
            }

            AddLog( { ConsoleLogLevel::eInfo, expanded } );
        } );

        RegisterCommand( "clear", "Clears all console messages",
            [this]( const eastl::vector<eastl::string>& ) {
            ClearLogs();
        } );

        RegisterCommand( "set", "Sets a CVar: set name value", [this]( const eastl::vector<eastl::string>& args ) {
            if ( args.size() < 2 ) {
                Warning( "Usage: set <name> <value>" );
                return;
            }

            eastl::string value{ args[1] };

            for ( core::usize index{ 2 }; index < args.size(); ++index ) {
                value += " ";
                value += args[index];
            }

            if ( !CVarRegistry::Get().Set( args[0], value ) ) {
                Error( string::Format( "Could not set CVar '{}': it is immutable or the value has the wrong type.", args[0] ) );
            }
        } );

        RegisterCommand( "get", "Prints a CVar: get name", [this]( const eastl::vector<eastl::string>& args ) {
            if ( args.size() != 1 ) {
                Warning( "Usage: get <name>" );
                return;
            }

            if ( !CVarRegistry::Get().Contains( args[0] ) ) {
                Warning( string::Format( "Unknown CVar '{}'.", args[0] ) );
                return;
            }

            Info( string::Format( "{} = {}", args[0], CVarRegistry::Get().GetString( args[0] ) ) );
        } );

        RegisterCommand( "toggle", "Toggles a boolean CVar: toggle name", [this]( const eastl::vector<eastl::string>& args ) {
            if ( args.size() != 1 ) {
                Warning( "Usage: toggle <name>" );
                return;
            }

            if ( !CVarRegistry::Get().Toggle( args[0] ) ) {
                Warning( string::Format( "Could not toggle CVar '{}'.", args[0] ) );
            }
        } );

        RegisterCommand( "/", "Executes an external system command",
            [this]( const eastl::vector<eastl::string>& args ) {
            if ( args.empty() ) {
                AddLog( { ConsoleLogLevel::eWarning, "Usage: /<command> [ARGS]" } );
                return;
            }

            // Pack arguments for the command
            eastl::string cmd{};

            for ( const auto& arg: args ) {
                cmd += arg + " ";
            }

            // Submit for execution
            AddLog( { ConsoleLogLevel::eDebug, "Running command: " + cmd } );
            process::RunAsync( cmd, [this]( const eastl::string& line ) {
                AddLog( { ConsoleLogLevel::eInfo, line } );
            } );
        } );
    }
}// namespace Mikoto
