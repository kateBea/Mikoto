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

#ifndef MIKOTO_MATERIAL_PARTICLE_MATERIAL_HH
#define MIKOTO_MATERIAL_PARTICLE_MATERIAL_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <yaml-cpp/yaml.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Filesystem/Path.hh>
#include <Filesystem/File.hh>

#include <Assets/AssetTexture.hh>
#include <Assets/AssetLoadRegistry.hh>

#include <Material/Material.hh>
#include <Material/MaterialGraph.hh>

namespace mikoto::material {

    struct ParticleMaterialDescription {
        core::float4 mColor{ 1.0f };
        eastl::string mName{ "ParticleMaterial" };
    };

    class ParticleMaterial final : public IMaterial {
    public:
        explicit ParticleMaterial( eastl::string_view name = "ParticleMaterial" );
        explicit ParticleMaterial( const ParticleMaterialDescription& description );

        ParticleMaterial( const ParticleMaterial& other );

        auto Deserialize( const filesystem::Path& filename ) -> void override;
        auto Serialize( const filesystem::Path& filename ) const -> void override;

        auto Deserialize( filesystem::FileHandle file ) -> void override;
        auto Serialize( filesystem::FileHandle file ) const -> void override;

        auto SetColor( const core::float4& value ) -> void;
        auto SetName( eastl::string_view newName ) -> void override;
        auto SetTexture( asset::AssetHandle<asset::Texture> texture ) -> void;

        MKT_NODISCARD auto GetColor() const -> const core::float4&;
        MKT_NODISCARD auto GetTexture() const -> renderer::rhi::TextureHandle;
        MKT_NODISCARD auto GetTexturePath() const -> filesystem::Path;

        MKT_NODISCARD auto GetGraph() -> MaterialGraph*;
        MKT_NODISCARD auto GetGraph() const -> const MaterialGraph*;

        MKT_NODISCARD auto GetName() const -> const eastl::string& override;

        MKT_NODISCARD auto GetSourcePath() const -> const filesystem::Path&;

    private:
        // [Internal usage]
        auto RestoreProperties( const YAML::Node& node ) -> void;
        MKT_NODISCARD auto SerializeProperties() const -> YAML::Node;

    private:
        eastl::string mName{};
        core::float4 mColor{ 1.0f };
        ParticleMaterialGraph mGraph{};

        // The bilboarded texture.
        asset::AssetHandle<asset::Texture> mTexture{};

        mutable filesystem::Path mSourcePath{};
    };

    using ParticleMaterialHandle = core::Ref<ParticleMaterial>;
}// namespace mikoto::material
#endif
