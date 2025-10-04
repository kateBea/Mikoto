//
// Created by kate on 1/4/25.


#ifndef CONFIGLOADER_HH
#define CONFIGLOADER_HH

#include <any>

#include <Common/Service.hh>
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

    class IConfiguration {
    public:
        virtual ~IConfiguration() = default;

        virtual auto Load( const Path &filePath ) -> void = 0;
    };

}// namespace Mikoto

#endif//CONFIGLOADER_HH
