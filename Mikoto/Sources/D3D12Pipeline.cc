//    Copyright 2025 ケイト
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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Renderer/Core/Rhi.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

// D3D12 extension library.
#include <directx/d3d12.h>

#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <wrl.h>

#include <Renderer/D3D12/D3D12Pipeline.hh>

namespace mikoto::renderer::d3d12 {

    using namespace mikoto::renderer::rhi;

    GraphicsPipeline::GraphicsPipeline( const rhi::GraphicsPipelineDescription &info )
        : IGraphicsPipeline{ info }
    {
    }

    auto GraphicsPipeline::GetNativeHandle( rhi::ObjectType type ) -> rhi::Object {
        return IGraphicsPipeline::GetNativeHandle( type );
    }

    auto GraphicsPipeline::GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object {
        return IGraphicsPipeline::GetNativeHandle( type );
    }

    auto GraphicsPipeline::SetDebugName( eastl::string_view name ) -> void {
        IGraphicsPipeline::SetDebugName( name );
    }

    GraphicsPipeline::~GraphicsPipeline() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto GraphicsPipeline::Initialize() -> void {
        mIsAllocated = true;
    }

    auto GraphicsPipeline::Release() -> void {
        mIsAllocated = false;
    }

    ComputePipeline::ComputePipeline( const rhi::ComputePipelineDescription &info )
        : IComputePipeline{ info }
    {
    }

    auto ComputePipeline::GetNativeHandle( rhi::ObjectType type ) -> rhi::Object {
        return IComputePipeline::GetNativeHandle( type );
    }

    auto ComputePipeline::GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object {
        return IComputePipeline::GetNativeHandle( type );
    }

    auto ComputePipeline::SetDebugName( eastl::string_view name ) -> void {
        IComputePipeline::SetDebugName( name );
    }

    ComputePipeline::~ComputePipeline() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto ComputePipeline::Initialize() -> void {
        mIsAllocated = true;
    }

    auto ComputePipeline::Release() -> void {
        mIsAllocated = false;
    }
}// namespace mikoto::renderer::d3d12

#endif