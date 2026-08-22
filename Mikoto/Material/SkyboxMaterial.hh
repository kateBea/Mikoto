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

#ifndef MIKOTO_SKYBOX_MATERIAL_HH
#define MIKOTO_SKYBOX_MATERIAL_HH

#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <ankerl/unordered_dense.h>

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

        renderer::rhi::TextureHandle mEquirectangularTextureHandle{};
        ankerl::unordered_dense::map<SkyboxFace, renderer::rhi::TextureHandle> mCubeFaces{};

        auto SetName( eastl::string_view name ) -> SkyboxMaterialDescription&;

        // Automatically sets type to equirectangular
        auto SetEquirectangular( renderer::rhi::TextureHandle texture ) -> SkyboxMaterialDescription&;

        // Automatically sets type to cubes
        auto SetCubeFace( SkyboxFace face, renderer::rhi::TextureHandle texture ) -> SkyboxMaterialDescription&;
    };

    class SkyboxMaterial final : public Material {
    public:
        explicit SkyboxMaterial( eastl::string_view name = "SkyboxMaterial" );
        explicit SkyboxMaterial( const SkyboxMaterialDescription& desc );

        auto SetFace( SkyboxFace face, renderer::rhi::TextureHandle texture ) -> void;

        auto SetType( SkyboxType type ) -> void;
        MKT_NODISCARD auto IsType( SkyboxType type ) const -> bool;
        MKT_NODISCARD auto GetType() const -> SkyboxType;

        MKT_NODISCARD auto GetFace(SkyboxFace face) -> renderer::rhi::TextureHandle;
        MKT_NODISCARD auto GetFaceTextures() -> ankerl::unordered_dense::map<SkyboxFace, renderer::rhi::TextureHandle>&;
        MKT_NODISCARD auto GetEquirectangular() -> renderer::rhi::TextureHandle;

        ~SkyboxMaterial() override = default;

    private:
        SkyboxType mType{ SkyboxType::eCubeFaces };
        renderer::rhi::TextureHandle mEquirectangular{};
        ankerl::unordered_dense::map<SkyboxFace, renderer::rhi::TextureHandle> mCubeFaces{};
    };
} // namespace mikoto

#endif//MIKOTO_SKYBOX_MATERIAL_HH
