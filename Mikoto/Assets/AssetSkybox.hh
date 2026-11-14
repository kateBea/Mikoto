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

#ifndef MIKOTO_ASSETS_ASSET_SKYBOX_HH
#define MIKOTO_ASSETS_ASSET_SKYBOX_HH

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Assets/Asset.hh>

#include <Material/SkyboxMaterial.hh>

namespace mikoto::asset {

    struct SkyboxLoadDescription {
        filesystem::Path mPath{};
        LoadPriority mPriority{ LoadPriority::eNormal };
    };

    /** Owns the runtime resource; scenes keep AssetRef/AssetHandle instead of backend objects. */
    class Skybox final : public core::ReferenceCounted<Skybox> {
    public:
        explicit Skybox( core::Ref<material::SkyboxMaterial> resource );
        ~Skybox() override;
        MKT_NODISCARD auto GetResource() const -> core::Ref<material::SkyboxMaterial>;

        MKT_NODISCARD auto CanEvict() const -> bool;
        MKT_NODISCARD auto GetResidentMemoryUsage() const -> AssetMemoryUsage;
    private:
        core::Ref<material::SkyboxMaterial> mResource{};
    };

    template<> struct AssetTraits<Skybox> {
        using Asset = Skybox;
        using Description = SkyboxLoadDescription;

        static constexpr AssetType kType{ AssetType::eSkybox };
    };

    template<>
    struct AssetTraits<material::SkyboxMaterial> : public AssetTraits<Skybox> {

    };
}// namespace mikoto::asset
#endif// MIKOTO_ASSETS_ASSET_SKYBOX_HH
