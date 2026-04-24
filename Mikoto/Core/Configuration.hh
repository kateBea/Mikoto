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

#ifndef MIKOTO_CONFIG_LOADER_HH
#define MIKOTO_CONFIG_LOADER_HH

#include <EASTL/any.h>
#include <EASTL/string.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Filesystem/Path.hh>

namespace mikoto::core {

    class Configuration {
    public:

        MKT_NODISCARD auto IsLoaded() const  -> bool { return mIsLoaded; }

        template<typename T>
        MKT_NODISCARD auto Get( const eastl::string& key, const T& defaultValue = {} ) const -> T {
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

#endif //MIKOTO_CONFIG_LOADER_HH
