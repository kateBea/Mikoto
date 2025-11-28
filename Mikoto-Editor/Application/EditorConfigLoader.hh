//
// Created by kate on 11/28/25.
//

#ifndef MIKOTO_EDITORCONFIGLOADER_HH
#define MIKOTO_EDITORCONFIGLOADER_HH

#include <any>
#include <string_view>

#include <toml++/toml.hpp>

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

#endif//MIKOTO_EDITORCONFIGLOADER_HH
