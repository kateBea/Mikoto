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

#ifndef MIKOTO_LUA_HELPERS_HH
#define MIKOTO_LUA_HELPERS_HH

#include <sol/sol.hpp>
#include <Logging/Logger.hh>

namespace mikoto::scripting {
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
