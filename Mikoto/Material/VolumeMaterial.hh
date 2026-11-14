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

#ifndef MIKOTO_MATERIAL_VOLUME_MATERIAL_HH
#define MIKOTO_MATERIAL_VOLUME_MATERIAL_HH

#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Material/Material.hh>

namespace mikoto::material {

    enum class VolumeDensityModel : core::u32 {
        eHomogeneous,
        eExponentialHeight,
        eProceduralCloud,
        eDensityGrid
    };

    // Coefficients are inverse world units, independent of the volume's transform.
    // PBRT medium properties: https://pbr-book.org/4ed/Volume_Scattering/Media
    struct VolumeSettings {
        VolumeDensityModel mDensityModel{ VolumeDensityModel::eHomogeneous };
        core::float3 mAbsorption{ 0.0f };
        core::float3 mScattering{ 0.01f };
        core::f32 mAnisotropy{ 0.0f };
        core::f32 mDensity{ 1.0f };
        core::f32 mHeightFalloff{ 0.1f };
        core::f32 mNoiseScale{ 0.003f };
        core::f32 mErosionScale{ 0.02f };
        core::f32 mErosion{ 0.2f };
        core::f32 mCoverage{ 0.5f };
        core::u32 mOctaves{ 5 };

    };

    class VolumeMaterial final : public IMaterial {
    public:
        explicit VolumeMaterial( eastl::string_view name = "Volume Material" );

        VolumeMaterial( const VolumeMaterial& other );

        auto SetName( eastl::string_view name ) -> void override;
        auto SetSettings( const VolumeSettings& settings ) -> void;

        // x changes fastest. Trilinear interpolation preserves the [0,1] density bound.
        auto SetDensityGrid( glm::uvec3 dimensions, const eastl::vector<core::f32>& values ) -> void;
        auto ImportDensityGrid( const filesystem::Path& filename ) -> void;

        MKT_NODISCARD auto GetName() const -> const eastl::string& override;
        MKT_NODISCARD auto GetSettings() const -> const VolumeSettings&;
        MKT_NODISCARD auto GetSourcePath() const -> const filesystem::Path&;

        MKT_NODISCARD auto GetGridDimensions() const -> glm::uvec3;
        MKT_NODISCARD auto GetDensityGrid() const -> const eastl::vector<core::f32>&;

        auto Serialize( const filesystem::Path& filename ) const -> void override;
        auto Serialize( filesystem::FileHandle file ) const -> void override;
        auto Deserialize( const filesystem::Path& filename ) -> void override;
        auto Deserialize( filesystem::FileHandle file ) -> void override;

    private:
        eastl::string mName{};
        VolumeSettings mSettings{};
        
        core::uint3 mGridDimensions{};
        eastl::vector<core::f32> mDensityGrid{};

        mutable filesystem::Path mSourcePath{};
    };

    using VolumeMaterialHandle = core::Ref<VolumeMaterial>;
}
#endif
