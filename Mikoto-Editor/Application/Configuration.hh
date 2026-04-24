//    Copyright 2025 ケイト
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

#ifndef MIKOTO_EDITOR_CONFIG_LOADER_HH
#define MIKOTO_EDITOR_CONFIG_LOADER_HH

#include <string>

#include <EASTL/any.h>
#include <EASTL/string.h>
#include <EASTL/utility.h>
#include <EASTL/string_view.h>

/**
 * Can disable exceptions in
 * compiler flags and/or explicitly disable the library's use of them by setting the option
 * #TOML_EXCEPTIONS to 0. In either case, the parsing functions return a
 * toml::parse_result instead of a toml::table:
 *
 *  only necessary if you've left them enabled in your compiler #include <toml++/toml.hpp>
 * */
#define TOML_EXCEPTIONS 0
#include <toml++/toml.hpp>
#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Exception.hh>
#include <Core/Configuration.hh>

#include <Filesystem/Path.hh>

namespace mikoto::editor {

    using namespace mikoto::core;

    class BaseConfiguration final : public Configuration {
    public:
        explicit BaseConfiguration( const filesystem::Path& filePath ) {
            Load( filePath );
        }

        explicit BaseConfiguration( eastl::string_view path ) {
            Load( filesystem::Path{ path } );
        }

        auto Load( const filesystem::Path& filePath ) -> void override {
            toml::parse_result result{ toml::parse_file( filePath.GetC_Str() ) };

            if ( result.failed() ) {
                MKT_THROW_RUNTIME_ERROR( string::Format( "Failed to load configuration file: {}", filePath.GetC_Str() ) );
            }

            mData.clear();

            const toml::table& tbl{ result.table() };
            for ( auto&& [sectionName, sectionValue]: tbl ) {
                if ( auto* section{ sectionValue.as_table() } ) {

                    for ( auto&& [key, value]: *section ) {
                        eastl::string keyFullName{ string::Format( "{}{}{}", sectionName.str(), kSeparator, key.str() ) };

                        value.visit( [&]( auto&& v ) {
                            mData[keyFullName] = ToBasicType( v );
                        } );
                    }
                }
            }

            mIsLoaded = true;
        }

    private:
        // Separator between section and key
        static constexpr eastl::string_view kSeparator{ "." };

        static auto ToBasicType( const auto& v ) -> eastl::any {
            using namespace mikoto;

            using VType = eastl::decay_t<decltype( v )>;

            if constexpr ( toml::is_boolean<VType> ) {
                return eastl::make_any<bool>( as<bool>( v ) );
            } else if constexpr ( toml::is_integer<VType> ) {
                return eastl::make_any<i64>( as<i64>( v )  );
            } else if constexpr ( toml::is_floating_point<VType> ) {
                return eastl::make_any<f64>( as<f64>( v ) );
            } else if constexpr ( toml::is_string<VType> ) {
                return eastl::make_any<eastl::string>( std::string{ v }.c_str() );
            }

            return eastl::any{};
        }
    };
}

#endif//MIKOTO_EDITOR_CONFIG_LOADER_HH