//
// Created by kate on 10/28/25.
//

#include "Core/ExecuteProcess.hh"

#include <Threading/TaskService.hh>
#include <array>
#include <cstdio>
#include <memory>
#include <thread>

#if defined( _WIN32 )
#define POPEN  _popen
#define PCLOSE _pclose
#else
#define POPEN  popen
#define PCLOSE pclose
#endif

namespace Mikoto {

    auto ExecuteProcess::Run( const std::string& command ) -> std::string {
#if defined( _WIN32 )
        std::string cmd = command + " 2>&1";
#else
        std::string cmd = command + " 2>&1";
#endif

        std::array<char, 256> buffer{};
        std::string result;

        if ( FILE* pipe = popen( cmd.c_str(), "r" ) ) {
            while ( fgets( buffer.data(), buffer.size(), pipe ) != nullptr ) {
                result += buffer.data();
            }
            pclose( pipe );
        }

        return result;
    }

    auto ExecuteProcess::RunDetached( const std::string& command ) -> int {
#if defined( _WIN32 )
        std::string cmd = "start /B " + command;
        return std::system( cmd.c_str() );
#else
        std::string cmd = command + " &";
        return std::system( cmd.c_str() );
#endif
    }

    auto ExecuteProcess::RunAsync( const std::string& command, std::function<void( const std::string& )> onOutput ) -> void {
        TaskService::Get()->Submit( [command, onOutput = std::move( onOutput )]() -> void {
#if defined( _WIN32 )
            std::string cmd = command + " 2>&1";
#else
            std::string cmd = command + " 2>&1";
#endif
            std::array<char, 256> buffer{};

            if ( FILE* pipe = popen( cmd.c_str(), "r" ) ) {
                while ( fgets( buffer.data(), buffer.size(), pipe ) != nullptr ) {
                    if ( onOutput ) {
                        onOutput( buffer.data() );
                    }
                }
                pclose( pipe );
            }
        } );
    }
}// namespace Mikoto