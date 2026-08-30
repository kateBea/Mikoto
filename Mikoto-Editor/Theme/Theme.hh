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

#ifndef MIKOTO_THEME_HH
#define MIKOTO_THEME_HH

#include <Core/Serializable.hh>

namespace mikoto::editor {

    class Theme final : core::ISerializable {
    public:

        auto Apply() -> void;

        auto Serialize( const filesystem::Path &filename ) const -> void override;
        auto Deserialize( const filesystem::Path &filename ) const -> void override;

        auto Serialize( filesystem::FileHandle file ) const -> void override;
        auto Deserialize( filesystem::FileHandle file ) const -> void override;

    private:
    };
}


#endif //MIKOTO_THEME_HH