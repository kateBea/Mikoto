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

#include <any>
#include <string_view>

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

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Core/Configuration.hh>

namespace Mikoto {
    class BaseConfiguration final : public Configuration {
    public:
        explicit BaseConfiguration( const Path& filePath ) {
            Load( filePath );
        }

        auto Load( const Mikoto::Path& filePath ) -> void override {
            toml::parse_result result{ toml::parse_file( filePath.string() ) };

            if ( result.failed() ) {
                MKT_THROW_RUNTIME_ERROR( fmt::format( "Failed to load configuration file: {}", filePath.string() ) );
            }

            m_Data.clear();

            const toml::table& tbl{ result.table() };
            for ( auto&& [sectionName, sectionValue]: tbl ) {
                if ( auto* section = sectionValue.as_table() ) {

                    for ( auto&& [key, value]: *section ) {
                        std::string fullKey = fmt::format( "{}{}{}", sectionName.str(), SEPARATOR, key.str() );

                        value.visit( [&]( auto&& v ) {
                            m_Data[fullKey] = ToNativeType( v );
                        } );
                    }
                }
            }

            m_IsLoaded = true;
        }

    private:
        // Separator between section and key
        static constexpr std::string_view SEPARATOR{ "." };

        /**
         * Converts a TOML value to std::any
         * @param v Toml value
         * @return std::any containing the value, or null if the type is unsupported
         */
        static auto ToNativeType( const auto& v ) -> std::any {
            using namespace Mikoto;

            using VType = std::decay_t<decltype( v )>;

            if constexpr ( toml::is_boolean<VType> ) {
                return std::make_any<bool>( v );
            } else if constexpr ( toml::is_integer<VType> ) {
                return std::make_any<Int64>( v );
            } else if constexpr ( toml::is_floating_point<VType> ) {
                return std::make_any<double>( v );
            } else if constexpr ( toml::is_string<VType> ) {
                return std::make_any<std::string>( v );
            }

            return std::any{};
        }
    };
}

#endif//MIKOTO_EDITOR_CONFIG_LOADER_HH
