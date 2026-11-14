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

#ifndef MIKOTO_RENDERER_RHI_D3D12_D3D12_ACCELERATION_STRUCTURE_HH
#define MIKOTO_RENDERER_RHI_D3D12_D3D12_ACCELERATION_STRUCTURE_HH

#include <Core/Platform.hh>

#include <Renderer/Rhi/AccelerationStructure.hh>
#include <Renderer/Rhi/D3D12/D3D12MemoryAllocator.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

namespace mikoto::renderer::d3d12 {
    
    

    class AccelerationStructure final : public rhi::IAccelerationStructure {
        friend class CommandList;

    public:
        explicit AccelerationStructure( const rhi::AccelStructureCreateDescription& description );
        ~AccelerationStructure() override;

        DISABLE_COPY_AND_MOVE_FOR( AccelerationStructure );

        auto GetAddress() const -> rhi::DeviceAddress override;
        auto GetType() const -> rhi::AccelStructureType override;
        auto IsBuilt() const -> bool;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

        renderer::rhi::AccelStructureCreateDescription mDescription{};
        rhi::BufferHandle mResult{};
        rhi::BufferHandle mScratch{};
        rhi::BufferHandle mInstances{};
        D3D12_RAYTRACING_GEOMETRY_DESC mGeometry{};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC mBuild{};
        bool mBuilt{};
    };
}

#endif

#endif// MIKOTO_RENDERER_RHI_D3D12_D3D12_ACCELERATION_STRUCTURE_HH

