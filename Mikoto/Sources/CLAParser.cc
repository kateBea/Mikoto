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
#include <vector>
#include <exception>

#include <Core/CLAParser.hh>

namespace mikoto::core {

    auto CLAParser::Parse( const int argc, char** argv ) -> bool {
        argparse::ArgumentParser parser{ "MikotoEditor" };
        parser.add_description( "Mikoto Editor startup options" );

        parser.add_argument( "--set" )
            .append()
            .metavar( "name=value" )
            .help( "Override a CVar. May be supplied more than once." );

        parser.add_argument( "--help" )
            .default_value( false )
            .implicit_value( true )
            .help( "Show this help text." );

        mCVarOverrides.clear();
        mMessage.clear();
        mHelpRequested = false;

        try {
            parser.parse_args( argc, argv );

            if ( parser.get<bool>( "--help" ) ) {
                mHelpRequested = true;
                mMessage = parser.help().str().c_str();
                return false;
            }

            if ( parser.is_used( "--set" ) ) {
                const auto& assignments{ parser.get<std::vector<std::string>>( "--set" ) };
                for ( const std::string& assignment : assignments ) {
                    mCVarOverrides.emplace_back( assignment.c_str() );
                }
            }
        } catch ( const std::exception& error ) {
            mMessage = error.what();
            return false;
        }

        return true;
    }

    auto CLAParser::GetCVarOverrides() const -> const eastl::vector<eastl::string>& {
        return mCVarOverrides;
    }

    auto CLAParser::GetMessageOutput() const -> const eastl::string& {
        return mMessage;
    }

    auto CLAParser::IsHelpRequested() const -> bool {
        return mHelpRequested;
    }
}
