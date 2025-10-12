//
// Created by kate on 1/4/25.


#ifndef CONFIGLOADER_HH
#define CONFIGLOADER_HH

#include <Common/Service.hh>
#include <Library/Utility/Types.hh>
#include <any>
#include <memory>
#include <string>

/**
 * Can disable exceptions in
 * compiler flags and/or explicitly disable the library's use of them by setting the option
 * #TOML_EXCEPTIONS to 0. In either case, the parsing functions return a
 * toml::parse_result instead of a toml::table:
 *
 *  only necessary if you've left them enabled in your compiler #include <toml++/toml.hpp>
 * */
#define TOML_EXCEPTIONS 0

#include <ankerl/unordered_dense.h>
#include <fmt/format.h>

#include <Logging/Logger.hh>
#include <toml++/toml.hpp>

namespace Mikoto {

    class Configuration {
    public:
        virtual ~Configuration() = default;

        virtual auto Load( const Path& filePath ) -> void = 0;

        template<typename T>
        auto Get( const std::string& key, const T& defaultValue = {} ) -> T {
            const auto it{ m_Data.find( key ) };
            if ( it != m_Data.end() ) {
                if ( auto* val{ std::any_cast<T>( &it->second ) } ) {
                    return *val;
                }
            }

            return defaultValue;
        }

    protected:
        ankerl::unordered_dense::map<std::string, std::any> m_Data{};
    };

    class BaseConfiguration final : public Configuration {
    public:
        explicit BaseConfiguration(const Path& filePath) {
            Load(filePath);
        }

        auto Load(const Path& filePath) -> void override {
            auto result = toml::parse_file(filePath.string());

            if (result.failed()) {
                MKT_THROW_RUNTIME_ERROR(fmt::format("Failed to load configuration file: {}", filePath.string()));
                return;
            }

            m_Data.clear();

            const auto& tbl = result.table();
            for (auto&& [sectionName, sectionValue] : tbl) {
                if (auto* section = sectionValue.as_table()) {

                    for (auto&& [key, value] : *section) {
                        std::string fullKey = fmt::format("{}{}{}", sectionName.str(), SEPARATOR, key.str());

                        value.visit([&](auto&& v) {
                            m_Data[fullKey] = ToAny(v);
                        });
                    }
                }
            }
        }

    private:
        // Separator between section and key
        static constexpr std::string_view SEPARATOR{ "." };

        /**
         * Converts a TOML value to std::any
         * @param v Toml value
         * @return std::any containing the value, or null if the type is unsupported
         */
        static auto ToAny(const auto& v) -> std::any {
            using VType = std::decay_t<decltype(v)>;

            if constexpr (toml::is_boolean<VType>) {
                return std::make_any<bool>(v);
            } else if constexpr (toml::is_integer<VType>) {
                return std::make_any<Int64>(v);
            } else if constexpr (toml::is_floating_point<VType>) {
                return std::make_any<double>(v);
            } else if constexpr (toml::is_string<VType>) {
                return std::make_any<std::string>(v);
            }

            return std::any{};
        }
    };

}// namespace Mikoto

#endif//CONFIGLOADER_HH
