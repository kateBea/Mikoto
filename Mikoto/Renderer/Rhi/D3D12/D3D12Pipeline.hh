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

#ifndef MIKOTO_RENDERER_RHI_D3D12_D3D12_PIPELINE_HH
#define MIKOTO_RENDERER_RHI_D3D12_D3D12_PIPELINE_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Pipeline.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

// D3D12 extension library.

#include <Platform/Windows/Windows.hh>

#include <directx/d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <wrl.h>

#include <EASTL/array.h>

#include <Renderer/Rhi/Pipeline.hh>
#include <Renderer/Rhi/D3D12/D3D12MemoryAllocator.hh>

namespace mikoto::renderer::d3d12 {

    // https://learn.microsoft.com/en-us/windows/win32/direct3d12/pipelines-and-shaders-with-directx-12
    class GraphicsPipeline final :  public rhi::GraphicsPipeline {
    public:
        explicit GraphicsPipeline( const rhi::GraphicsPipelineDescription& info );

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        auto SetDebugName( eastl::string_view name ) -> void override;

        // D312 Specifics
        MKT_NODISCARD auto HasEmptyRootSignature() const -> bool;
        MKT_NODISCARD operator ID3D12PipelineState*() const;

        ~GraphicsPipeline() override;

    public:
        DISABLE_COPY_AND_MOVE_FOR( GraphicsPipeline );

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        Microsoft::WRL::ComPtr<ID3D12PipelineState> mPipelineState{};
        D3D12_GRAPHICS_PIPELINE_STATE_DESC mD3D12PipelineDesc{};

        bool mRequiresEmptySignature{};
    };

    class ComputePipeline final :  public rhi::ComputePipeline {
    public:
        explicit ComputePipeline( const rhi::ComputePipelineDescription& info );

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        auto SetDebugName( eastl::string_view name ) -> void override;

        // D312 Specifics
        MKT_NODISCARD auto HasEmptyRootSignature() const -> bool;
        MKT_NODISCARD operator ID3D12PipelineState*() const;

        ~ComputePipeline() override;

    public:
        DISABLE_COPY_AND_MOVE_FOR( ComputePipeline );

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        Microsoft::WRL::ComPtr<ID3D12PipelineState> mPipelineState{};
        D3D12_COMPUTE_PIPELINE_STATE_DESC mD3D12PipelineDesc{};

        bool mRequiresEmptySignature{};
    };

    class RayTracingPipeline final : public rhi::RayTracingPipeline {
    public:
        explicit RayTracingPipeline( const rhi::RayTracingPipelineDescription& description );

        ~RayTracingPipeline() override;

        DISABLE_COPY_AND_MOVE_FOR( RayTracingPipeline );

    private:
        auto Initialize() -> void override;

        auto Destroy() -> void override;

        friend class CommandList;
        friend class ShaderTable;

        Microsoft::WRL::ComPtr<ID3D12StateObject> mPipeline{};

        // Unique state-object exports in ray-generation, miss and closest-hit order.
        eastl::array<eastl::wstring, 3> mShaderExports{};

    };

}

#endif

#endif// MIKOTO_RENDERER_RHI_D3D12_D3D12_PIPELINE_HH
