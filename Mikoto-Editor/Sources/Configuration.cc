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

#include <Application/Configuration.hh>

namespace mikoto::editor {

    BaseConfiguration::BaseConfiguration( const filesystem::Path& filePath ) {
        MKT_BEGIN_PROFILER();
        Load( filePath );
    }


    BaseConfiguration::BaseConfiguration( eastl::string_view path ) {
        MKT_BEGIN_PROFILER();
        Load( filesystem::Path{ path } );
    }

    auto BaseConfiguration::Load( const filesystem::Path& filePath ) -> void {
        MKT_BEGIN_PROFILER();
        toml::parse_result result{ toml::parse_file( filePath.GetC_Str() ) };

        if ( result.failed() ) {
            const auto& error{ result.error() };
            MKT_THROW_RUNTIME_ERROR( string::Format( "Configuration '{}', line {}, column {}: {}",
                filePath.GetC_Str(), error.source().begin.line, error.source().begin.column, error.description() ) );
        }

        // Build a complete replacement first. A failed reload leaves the
        // previous configuration usable. Support root values and arbitrarily
        // nested tables instead of silently dropping everything below level 2.
        decltype( mData ) loaded{};
        const auto flatten = [&]( auto&& self, const toml::table& table, const eastl::string& prefix ) -> void {
            for ( const auto& [key, value]: table ) {
                const auto name{ prefix.empty() ? eastl::string{ key.str().data(), key.str().size() } : string::Format( "{}.{}", prefix, key.str() ) };

                if ( const auto* nested{ value.as_table() } ) {
                    self( self, *nested, name );
                }
                else {
                    eastl::any scalar{};
                    value.visit( [&]( const auto& item ) {
                        scalar = ToBasicType( item );
                    } );

                    if ( !scalar.has_value() ) {
                        MKT_THROW_RUNTIME_ERROR( string::Format( "Configuration '{}': '{}' must be a boolean, number or string.", filePath.GetC_Str(), name ) );
                    }

                    if ( !loaded.try_emplace( name, eastl::move( scalar ) ).second ) {
                        MKT_THROW_RUNTIME_ERROR( string::Format( "Configuration '{}': ambiguous dotted key '{}'.", filePath.GetC_Str(), name ) );
                    }
                }
            }
        };
        flatten( flatten, result.table(), {} );
        mData = eastl::move( loaded );
        mIsLoaded = true;
    }
}