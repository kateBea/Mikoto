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

#ifndef MIKOTO_ASSETS_ASSET_MATERIAL_HH
#define MIKOTO_ASSETS_ASSET_MATERIAL_HH

#include <Assets/Asset.hh>

#include <Material/Material.hh>

namespace mikoto::asset {

    struct MaterialLoadDescription {
        filesystem::Path mPath{};
        LoadPriority mPriority{ LoadPriority::eNormal };
    };

    class Material final : public core::ReferenceCounted<Material> {
    public:
        explicit Material( core::Ref<material::IMaterial> resource );

        MKT_NODISCARD auto CanEvict() const -> bool; 
        MKT_NODISCARD auto GetName() const -> const eastl::string&;
        MKT_NODISCARD auto GetResource() const -> core::Ref<material::IMaterial>;
        MKT_NODISCARD auto GetResidentMemoryUsage() const -> AssetMemoryUsage;
        
        ~Material() override;
    private:
        core::Ref<material::IMaterial> mResource{};
    };

    template<> struct AssetTraits<Material> {
        using Asset = Material;
        using Description = MaterialLoadDescription;

        static constexpr AssetType kType{ AssetType::eMaterial };
    };

    template<>
    struct AssetTraits<material::IMaterial> : public AssetTraits<Material> {

    };
}// namespace mikoto::asset
#endif// MIKOTO_ASSETS_ASSET_MATERIAL_HH
