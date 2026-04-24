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

#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Profiler.hh>

#include <Renderer/Core/Renderer.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/SceneRenderer.hh>

namespace mikoto::renderer {

    SceneRenderer::SceneRenderer( const SceneRendererCreateInfo &createInfo )
        : mDevice{ createInfo.mDevice } {}

    auto SceneRenderer::Init() -> void {
        mFrameGraph = FrameGraph::Create( mDevice );

        // Initialize passes
        mDebugPasses.RegisterPasses( *mFrameGraph );

        // Build graph
        mFrameGraph->Compile();
    }

    auto SceneRenderer::Shutdown() -> void {
        mCamera = nullptr;
        mDevice = nullptr;
    }

    auto SceneRenderer::Render( Scene* scene ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        mFrameGraph->Execute();
    }

    auto SceneRenderer::SetMainCamera( SceneCamera *camera ) -> void {

    }

    auto SceneRenderer::SetClearColor( const float4 &color ) -> void {

    }

    auto SceneRenderer::GetTexture( eastl::string_view name ) const -> TextureHandle {
        return TextureHandle::CreateEmpty();
    }

    auto SceneRenderer::GetBuffer( eastl::string_view name ) const -> BufferHandle {
        return BufferHandle::CreateEmpty();
    }

    auto SceneRenderer::Create( const SceneRendererCreateInfo &spec ) -> eastl::unique_ptr<SceneRenderer> {
        return eastl::make_unique<SceneRenderer>( spec );
    }

    auto SceneRendererCreateInfo::WithName( eastl::string_view name ) -> SceneRendererCreateInfo & {
        this->mName = name;
        return *this;
    }

    auto SceneRendererCreateInfo::WithDevice( GpuDevice *device ) -> SceneRendererCreateInfo & {
        this->mDevice = device;
        return *this;
    }
}// namespace Mikoto