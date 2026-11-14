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

#ifndef MIKOTO_MATERIAL_TEXT_MATERIAL_HH
#define MIKOTO_MATERIAL_TEXT_MATERIAL_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <yaml-cpp/yaml.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Material/Material.hh>

namespace mikoto::material {

    class TextMaterial final : public IMaterial {
    public:
        explicit TextMaterial( eastl::string_view name = "TextMaterial" );

        TextMaterial( const TextMaterial& other );

        auto Deserialize( const filesystem::Path& filename ) -> void override;
        auto Serialize( const filesystem::Path& filename ) const -> void override;

        auto Deserialize( filesystem::FileHandle file ) -> void override;
        auto Serialize( filesystem::FileHandle file ) const -> void override;

        auto SetOpacity( core::f32 opacity ) -> void;
        auto SetColor( const core::float4& color ) -> void;
        auto SetName( eastl::string_view newName ) -> void override;

        MKT_NODISCARD auto GetOpacity() const -> core::f32;
        MKT_NODISCARD auto GetColor() const -> const core::float4&;
        MKT_NODISCARD auto GetSourcePath() const -> const filesystem::Path&;
        MKT_NODISCARD auto Evaluate( const core::float4& tint ) const -> core::float4;

        MKT_NODISCARD auto GetName() const -> const eastl::string& override;

        ~TextMaterial() override;

    private:
        // [Internal usage]
        MKT_NODISCARD auto SerializeProperties() const -> YAML::Node;
        auto RestoreProperties( const YAML::Node& node ) -> void;

    private:
        eastl::string mName{};
        core::float4 mColor{ 1.0f };
        core::f32 mOpacity{ 1.0f };

        mutable filesystem::Path mSourcePath{};
    };
}

#endif// MIKOTO_MATERIAL_TEXT_MATERIAL_HH
