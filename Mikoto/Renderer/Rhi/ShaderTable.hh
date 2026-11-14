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

#ifndef MIKOTO_RENDERER_RHI_SHADER_TABLE_HH
#define MIKOTO_RENDERER_RHI_SHADER_TABLE_HH

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Pipeline.hh>
#include <Renderer/Rhi/DeviceObject.hh>

namespace mikoto::renderer::rhi {

    // Current ray pipelines expose one ray-generation export, one miss export
    // and one triangle hit group. Each region repeats its pipeline export.
    struct ShaderTableDescription {
        IPipeline* mPipeline{ nullptr };
        core::u32 mMissRecordCount{ 1 };
        core::u32 mHitGroupRecordCount{ 1 };

        bool mTrackState{ true };

        constexpr auto SetPipeline( IPipeline* pipeline ) -> ShaderTableDescription& { mPipeline = pipeline; return *this; }
        constexpr auto SetMissRecordCount( core::u32 count ) -> ShaderTableDescription& {  mMissRecordCount = count;  return *this; }
        constexpr auto SetHitGroupRecordCount( core::u32 count ) -> ShaderTableDescription& { mHitGroupRecordCount = count; return *this; }
    };

    // Creation initializes the complete, immutable table. To change record
    // counts or the pipeline, create another table; recorded work retains its table.
    class IShaderTable : public DeviceObject {
    public:
        MKT_NODISCARD virtual auto GetPipeline() const -> IPipeline* = 0;
        MKT_NODISCARD virtual auto GetNumEntries() const -> core::u32 = 0;
        MKT_NODISCARD virtual auto GetDesc() const -> const ShaderTableDescription& = 0;
    };

    using ShaderTableHandle = core::Ref<IShaderTable>;

}// namespace mikoto::renderer::rhi

#endif// MIKOTO_RENDERER_RHI_SHADER_TABLE_HH
