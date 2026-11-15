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

#ifndef MIKOTO_MATERIAL_SKYBOX_MATERIAL_HH
#define MIKOTO_MATERIAL_SKYBOX_MATERIAL_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <yaml-cpp/yaml.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Assets/AssetTexture.hh>

#include <Material/Material.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Texture.hh>

namespace mikoto::material {

    enum class SkyboxFace {
        eFront,
        eBack,
        eLeft,
        eRight,
        eTop,
        eBottom
    };

    // Can either be an equirectangular
    // HDR image or a split into 6 faces
    enum class SkyboxType {
        eCubeFaces,
        eEquirectangular,
        eCount,
    };

    struct SkyboxMaterialDescription {
        eastl::string_view mName{};
        SkyboxType mSkyboxType{ SkyboxType::eEquirectangular };

        asset::AssetHandle<asset::Texture> mEquirectangularTextureHandle{};
        ankerl::unordered_dense::map<SkyboxFace, asset::AssetHandle<asset::Texture>> mCubeFaces{};

        auto SetName( eastl::string_view name ) -> SkyboxMaterialDescription&;

        // Automatically sets type to equirectangular
        auto SetEquirectangular( asset::AssetHandle<asset::Texture> texture ) -> SkyboxMaterialDescription&;

        // Automatically sets type to cubes
        auto SetCubeFace( SkyboxFace face, asset::AssetHandle<asset::Texture> texture ) -> SkyboxMaterialDescription&;
    };

    class SkyboxMaterial final : public IMaterial {
    public:
        explicit SkyboxMaterial( eastl::string_view name = "SkyboxMaterial" );
        explicit SkyboxMaterial( const SkyboxMaterialDescription& desc );

        SkyboxMaterial( const SkyboxMaterial& other );

        auto Serialize( const filesystem::Path &filename ) const -> void override;
        auto Deserialize( const filesystem::Path &filename ) -> void override;
        auto Serialize( filesystem::FileHandle file ) const -> void override;
        auto Deserialize( filesystem::FileHandle file ) -> void override;

        auto SetEquirectangular( asset::AssetHandle<asset::Texture> texture ) -> void;
        auto SetFace( SkyboxFace face, asset::AssetHandle<asset::Texture> texture ) -> void;

        auto SetAmbientScale( core::f32 scale ) -> void;
        
        auto SetType( SkyboxType type ) -> void;
        auto SetName( eastl::string_view newName ) -> void override;

        MKT_NODISCARD auto GetSourcePath() const -> const filesystem::Path&;

        // For the given type of skybox textures, it specifies if all of them are present
        // if equirectangular a flat image must be present (an HDR) otherwise for faces type
        // all six faces must be present
        MKT_NODISCARD auto HasRequiredTextures() const -> bool;

        MKT_NODISCARD auto IsType( SkyboxType type ) const -> bool;
        MKT_NODISCARD auto GetType() const -> SkyboxType;

        MKT_NODISCARD auto GetAmbientScale() const -> core::f32;

        MKT_NODISCARD auto GetFace( SkyboxFace face ) const -> renderer::rhi::TextureHandle;
        MKT_NODISCARD auto GetFaceTextures() const -> const ankerl::unordered_dense::map<SkyboxFace, asset::AssetHandle<asset::Texture>>&;
        MKT_NODISCARD auto GetEquirectangular() const -> renderer::rhi::TextureHandle;
        MKT_NODISCARD auto GetEquirectangularAsset() const -> const asset::AssetHandle<asset::Texture>&;
        MKT_NODISCARD auto GetFaceAsset( SkyboxFace face ) const -> asset::AssetHandle<asset::Texture>;

        MKT_NODISCARD auto GetName() const -> const eastl::string& override;

        ~SkyboxMaterial() override = default;

    private:
        // YAML is an implementation detail of the ISerializable entry points.
        MKT_NODISCARD auto SerializeProperties() const -> YAML::Node;
        auto RestoreProperties( const YAML::Node& node ) -> void;

        mutable filesystem::Path mSourcePath{};

        eastl::string mName{};
        core::f32 mAmbientScale{ 1.0f };

        SkyboxType mType{ SkyboxType::eCubeFaces };
        asset::AssetHandle<asset::Texture> mEquirectangular{};
        ankerl::unordered_dense::map<SkyboxFace, asset::AssetHandle<asset::Texture>> mCubeFaces{};
    };
} // namespace mikoto

#endif// MIKOTO_MATERIAL_SKYBOX_MATERIAL_HH
