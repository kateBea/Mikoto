//
// Created by kate on 1/4/25.


#ifndef CONFIGLOADER_HH
#define CONFIGLOADER_HH

#include <any>
#include <string>

#include <Library/Utility/Types.hh>

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

namespace Mikoto {

    /**
     * Mikoto exposes toml for config loading but user can use any
     *
     */
    class Configuration {
    public:
        virtual ~Configuration() = default;

        virtual auto Load( const Path& filePath ) -> void = 0;

        template<typename T>
        auto Get( const std::string& key, const T& defaultValue = {} ) const -> T {
            const auto it{ m_Data.find( key ) };
            if ( it != m_Data.end() ) {
                if ( auto* val{ std::any_cast<T>( &it->second ) } ) {
                    return *val;
                }
            }

            return defaultValue;
        }

        auto IsLoaded() const  -> bool { return m_IsLoaded; }

    protected:
        bool m_IsLoaded{ false };
        ankerl::unordered_dense::map<std::string, std::any> m_Data{};
    };

}// namespace Mikoto

#endif//CONFIGLOADER_HH
