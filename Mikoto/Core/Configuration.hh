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

#include <any>
#include <string>

#include <ankerl/unordered_dense.h>

#include <Library/Utility/Types.hh>

namespace Mikoto {

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

        MKT_NODISCARD auto IsLoaded() const  -> bool { return m_IsLoaded; }

    protected:
        bool m_IsLoaded{ false };
        ankerl::unordered_dense::map<std::string, std::any> m_Data{};
    };

}// namespace Mikoto

#endif //MIKOTO_CONFIG_LOADER_HH
