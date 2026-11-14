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

#ifndef MIKOTO_CORE_CONFIGURATION_HH
#define MIKOTO_CORE_CONFIGURATION_HH

#include <EASTL/allocator.h>
#include <EASTL/utility.h>
#include <EASTL/any.h>
#include <EASTL/string.h>
#include <EASTL/algorithm.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Profiler.hh>

#include <Filesystem/Path.hh>

namespace mikoto::core {

    class Configuration {
    public:
        MKT_NODISCARD auto IsLoaded() const -> bool;

        template<typename T>
        MKT_NODISCARD auto Get( const eastl::string& key, const T& defaultValue = {} ) const -> T {
            MKT_BEGIN_PROFILER();
            const auto it{ mData.find( key ) };
            if ( it != mData.end() ) {
                if ( auto* val{ eastl::any_cast<T>( &it->second ) } ) {
                    return *val;
                }
            }

            return defaultValue;
        }

        virtual auto Load( const filesystem::Path& filePath ) -> void = 0;

        virtual ~Configuration() = default;

    protected:
        ankerl::unordered_dense::map<eastl::string, eastl::any> mData{};
        bool mIsLoaded{ false };
    };

}// namespace Mikoto

#endif// MIKOTO_CORE_CONFIGURATION_HH
