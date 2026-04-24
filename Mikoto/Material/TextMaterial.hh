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

#ifndef MIKOTO_TEXT_MATERIAL_HH
#define MIKOTO_TEXT_MATERIAL_HH

#include <string_view>

#include <Material/Material.hh>

namespace mikoto::material {

    class TextMaterial final : public Material {
    public:

        explicit TextMaterial( std::string_view name = "TextMaterial" );

        ~TextMaterial() override;
    };
}

#endif//MIKOTO_TEXT_MATERIAL_HH
