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

#ifndef MIKOTO_COMMAND_LINE_PARSER_HH
#define MIKOTO_COMMAND_LINE_PARSER_HH

#include <EASTL/string.h>
#include <EASTL/vector.h>

#include <Core/Core.hh>

#include <argparse/argparse.hpp>

namespace mikoto::core {

    /**
     * @brief Parses Mikoto's startup command line into deferred CVar overrides.
     *
     * The parser deliberately records overrides instead of applying them directly.
     * This lets the executable parse arguments before engine services, the renderer,
     * or their CVars have been created.
     */
    class CLAParser final {
    public:
        /**
         * @brief Parses command-line arguments.
         *
         * @param argc Number of command-line arguments.
         * @param argv Command-line argument array.
         * @returns True when parsing succeeds; false when help or an error was requested.
         */
        auto Parse( int argc, char** argv ) -> bool;

        /**
         * @brief Returns CVar assignments supplied through repeated @c --set name=value arguments.
         *
         * @returns Deferred CVar assignments.
         */
        MKT_NODISCARD auto GetCVarOverrides() const -> const eastl::vector<eastl::string>&;

        /**
         * @brief Returns parser help text or the most recent parsing error.
         *
         * @returns Human-readable parser output.
         */
        MKT_NODISCARD auto GetMessageOutput() const -> const eastl::string&;

        /**
         * @brief Returns whether the parser stopped because help was requested.
         *
         * @returns True when @c --help was supplied.
         */
        MKT_NODISCARD auto IsHelpRequested() const -> bool;

    private:
        eastl::vector<eastl::string> mCVarOverrides{};
        eastl::string mMessage{};
        bool mHelpRequested{ false };
    };
}

#endif //MIKOTO_COMMAND_LINE_PARSER_HH
