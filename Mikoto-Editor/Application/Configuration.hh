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

#ifndef MIKOTO_APPLICATION_CONFIGURATION_HH
#define MIKOTO_APPLICATION_CONFIGURATION_HH

#include <string>

#include <EASTL/allocator.h>
#include <EASTL/utility.h>
#include <EASTL/any.h>
#include <EASTL/string.h>
#include <EASTL/algorithm.h>
#include <EASTL/string_view.h>

#include <Core/Profiler.hh>

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

#include <toml++/toml.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Exception.hh>
#include <Core/Configuration.hh>

#include <Filesystem/Path.hh>

namespace mikoto::editor {

    class BaseConfiguration final : public core::Configuration {
    public:
        BaseConfiguration() = default;

        explicit BaseConfiguration( const filesystem::Path& filePath );

        explicit BaseConfiguration( eastl::string_view path );

        auto Load( const filesystem::Path& filePath ) -> void override;

    private:
        static auto ToBasicType( const auto& v ) -> eastl::any {
            MKT_BEGIN_PROFILER();
            

            using VType = eastl::decay_t<decltype( v )>;

            if constexpr ( toml::is_boolean<VType> ) {
                return eastl::make_any<bool>( core::as<bool>( v ) );
            } else if constexpr ( toml::is_integer<VType> ) {
                return eastl::make_any<core::i64>( core::as<core::i64>( v )  );
            } else if constexpr ( toml::is_floating_point<VType> ) {
                return eastl::make_any<core::f64>( core::as<core::f64>( v ) );
            } else if constexpr ( toml::is_string<VType> ) {
                return eastl::make_any<eastl::string>( std::string{ v }.c_str() );
            }

            return eastl::any{};
        }
    };
}

#endif// MIKOTO_APPLICATION_CONFIGURATION_HH

