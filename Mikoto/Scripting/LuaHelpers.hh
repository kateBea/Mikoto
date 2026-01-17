//
// Created by kate on 1/17/26.
//

#ifndef MIKOTO_LUA_HELPERS_HH
#define MIKOTO_LUA_HELPERS_HH

#include <sol/sol.hpp>
#include <Logging/Logger.hh>

namespace Mikoto {
#define MKT_SOL_CALL( fn, ... )                                                  \
    do {                                                                         \
        if ( ( fn ).valid() ) {                                                  \
            sol::protected_function protected_fn = ( fn );                       \
            sol::protected_function_result result = protected_fn( __VA_ARGS__ ); \
            if ( !result.valid() ) {                                             \
                sol::error err = result;                                         \
                MKT_CORE_LOGGER_ERROR( "Lua Error: {}", err.what() );            \
            }                                                                    \
        }                                                                        \
    } while ( false )


}// namespace Mikoto

#endif//MIKOTO_LUA_HELPERS_HH
