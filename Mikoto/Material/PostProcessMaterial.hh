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

#ifndef MIKOTO_MATERIAL_POST_PROCESS_MATERIAL_HH
#define MIKOTO_MATERIAL_POST_PROCESS_MATERIAL_HH

#include <EASTL/string_view.h>

#include <yaml-cpp/yaml.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Material/Material.hh>
#include <Material/MaterialGraph.hh>

#include <Renderer/Rhi/Types.hh>

namespace mikoto::material {

    struct PostProcessMaterialDescription {
        eastl::string_view mName{};
    };

    enum class PostProcessStage : core::u8 {
        eBeforeToneMapping,
        eAfterToneMapping
    };

    enum class PostProcessMaterialCategory : core::u8 {
        eBuiltIn,
        eCustom
    };

    // This will be used for effects applied on top of an image
    // a good example is the chromatic aberration effect,
    // takes as input the final HDR shading image
    // the idea is that an effect like this is achieved only by tweaking
    // parameters from this material and apply those properties on the target image
    // See the PostProcessModule in the frame graph
    class PostProcessMaterial final : public IMaterial {
    public:
        explicit PostProcessMaterial( eastl::string_view name = "PostProcessMaterial" );
        explicit PostProcessMaterial( const PostProcessMaterialDescription& desc );

        PostProcessMaterial( const PostProcessMaterial& other );

        auto Deserialize( const filesystem::Path &filename ) -> void override;
        auto Serialize( const filesystem::Path &filename ) const -> void override;

        auto Deserialize( filesystem::FileHandle file ) -> void override;
        auto Serialize( filesystem::FileHandle file ) const -> void override;

        auto SetContrast( core::f32 value ) -> void;
        auto SetSaturation( core::f32 value ) -> void;
        auto SetTintColor( const core::float4& value ) -> void;

        auto SetStage( PostProcessStage stage ) -> void;
        auto SetFilmGrainEnabled( bool enabled ) -> void;

        auto SetName( eastl::string_view newName ) -> void override;
        auto SetCategory( PostProcessMaterialCategory category ) -> void;

        MKT_NODISCARD auto GetName() const -> const eastl::string& override;

        MKT_NODISCARD auto IsFilmGrainEnabled() const -> bool;
        MKT_NODISCARD auto GetStage() const -> PostProcessStage;

        MKT_NODISCARD auto GetCategory() const -> PostProcessMaterialCategory;

        MKT_NODISCARD auto GetSourcePath() const -> const filesystem::Path&;

        MKT_NODISCARD auto GetContrast() const -> core::f32;
        MKT_NODISCARD auto GetSaturation() const -> core::f32;
        MKT_NODISCARD auto GetTintColor() const -> const core::float4&;
        MKT_NODISCARD auto Evaluate( const core::float3& color ) const -> core::float3;

        MKT_NODISCARD auto GetGraph() -> MaterialGraph*;
        MKT_NODISCARD auto GetGraph() const -> const MaterialGraph*;

        ~PostProcessMaterial() override;

    private:
        // [Internal usage]
        MKT_NODISCARD auto SerializeProperties() const -> YAML::Node;
        auto RestoreProperties( const YAML::Node& node ) -> void;

    private:
        eastl::string mName{};
        core::f32 mContrast{ 1.0f };
        core::f32 mSaturation{ 1.0f };
        core::float4 mTintColor{ 1.0f };

        bool mFilmGrainEnabled{};
        PostProcessStage mStage{ PostProcessStage::eBeforeToneMapping };
        PostProcessMaterialCategory mCategory{ PostProcessMaterialCategory::eBuiltIn };

        PostprocessMaterialGraph mGraph{};

        // Chromatic aberration
        core::f32 mChromaIntensity{};
        core::f32 mChromaDirection{};
        core::f32 mRedChannelOffset{};
        core::f32 mBlueChannelOffset{};
        core::f32 mGreenChannelOffset{};

        mutable filesystem::Path mSourcePath{};
    };

}// namespace Mikoto

#endif// MIKOTO_MATERIAL_POST_PROCESS_MATERIAL_HH
