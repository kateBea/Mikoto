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

#include <array>
#include <cstdio>
#include <memory>

#include <Core/ExecuteProcess.hh>
#include <Threading/TaskService.hh>

// Posix names deprecated on Windows
#if defined( _WIN32 )
#define POPEN  _popen
#define PCLOSE _pclose
#else
#define POPEN  popen
#define PCLOSE pclose
#endif

namespace Mikoto {

    auto ExecuteProcess::Run( const std::string &command ) -> std::string {
        // Redirect stderr to stdout to merge both because
        // POPEN will have associated the process stdout or stdin (stderr is excluded)
        const std::string cmd{ command + " 2>&1" };
        constexpr std::string_view openMode{ "r" };

        std::string result{};
        std::array<char, 256> buffer{};

        if (std::FILE *pipe{ POPEN( cmd.c_str(), openMode.data() ) }) {
            while (std::fgets( buffer.data(), buffer.size(), pipe ) != nullptr) { result += buffer.data(); }
            PCLOSE( pipe );
        }

        return result;
    }

    auto ExecuteProcess::RunDetached( const std::string &command ) -> int {
#if defined( _WIN32 )
        const std::string cmd{ "start /B " + command };
#else
        const std::string cmd{ command + " &" };
#endif

        return std::system( cmd.c_str() );
    }

    auto ExecuteProcess::RunAsync( const std::string &command, AsyncCallback &&onOutput ) -> void {
        TaskService::Get()->Submit( [command, onOutput = std::move( onOutput )]() -> void {
            // See comment about redirection at ExecuteProcess::Run(...)
            const std::string cmd{ command + " 2>&1" };
            constexpr std::string_view openMode{ "r" };

            std::array<char, 256> buffer{};

            if (std::FILE *pipe{ POPEN( cmd.c_str(), openMode.data() ) }) {
                while (std::fgets( buffer.data(), buffer.size(), pipe ) != nullptr) {
                    if (onOutput) {
                        onOutput( buffer.data() );
                    }
                }

                PCLOSE( pipe );
            }
        } );
    }
}// namespace Mikoto