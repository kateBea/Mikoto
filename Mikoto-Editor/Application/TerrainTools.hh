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

#ifndef MIKOTO_EDITOR_APPLICATION_TERRAIN_TOOLS_HH
#define MIKOTO_EDITOR_APPLICATION_TERRAIN_TOOLS_HH

#include <EASTL/vector.h>

#include <Scene/Component.hh>
#include <Scene/Scene.hh>

namespace mikoto::editor {

    enum class TerrainEditMode { eGenerate,
        eSculpt,
        ePaint };
    enum class TerrainSculptTool { eRaiseLower,
        eSmooth,
        eFlatten };

    struct TerrainBrushSettings {
        TerrainEditMode mMode{ TerrainEditMode::eGenerate };
        TerrainSculptTool mSculptTool{ TerrainSculptTool::eRaiseLower };
        core::u32 mLayer{};
        core::f32 mRadius{ 12.0f };
        core::f32 mStrength{ .5f };
        core::f32 mFalloff{ .5f };
    };

    struct TerrainEditSnapshot {
        eastl::shared_ptr<const renderer::TerrainHeightmap> mHeightmap{};
        eastl::shared_ptr<const renderer::TerrainPaintMap> mPaintMap{};
    };

    struct TerrainStroke {
        TerrainEditSnapshot mBefore{};
        TerrainEditSnapshot mAfter{};
    };

    /** Shared authoring state; each completed mouse stroke is one undo operation. */
    struct TerrainToolState {
        auto SetTarget( scene::Scene *scene, core::u64 entity ) -> void;
        auto BeginStroke( const scene::LandscapeComponent &landscape, core::f32 height ) -> void;
        auto EndStroke( const scene::LandscapeComponent &landscape ) -> void;
        auto Undo( scene::LandscapeComponent &landscape ) -> void;
        auto Redo( scene::LandscapeComponent &landscape ) -> void;

        TerrainBrushSettings mBrush{};
        bool mEnabled{};
        bool mStrokeActive{};
        core::f32 mFlattenHeight{};
        scene::SceneHandle mScene{};
        core::u64 mEntity{};
        TerrainEditSnapshot mBefore{};
        eastl::vector<TerrainStroke> mHistory{};
        core::usize mHistoryIndex{};
    };

    /** Edits owned maps in metres. Shift lowers height or erases the selected layer. */
    auto ApplyTerrainBrush( scene::LandscapeComponent &landscape, const core::float4x4 &transform,
        const TerrainBrushSettings &brush, core::float3 center, core::f32 elapsed,
        bool reverse, core::f32 flattenHeight ) -> bool;

}// namespace mikoto::editor

#endif// MIKOTO_EDITOR_APPLICATION_TERRAIN_TOOLS_HH
