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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Profiler.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/Renderer.hh>

#include <Renderer/Core/ThumbnailRenderer.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::asset;
    using namespace mikoto::filesystem;
    using namespace mikoto::renderer::rhi;

    auto ThumbnailRendererCreateInfo::SetName( eastl::string_view name ) -> ThumbnailRendererCreateInfo & {
        mName = name;
        return *this;
    }

    auto ThumbnailRendererCreateInfo::SetDevice( IGpuDevice *device ) -> ThumbnailRendererCreateInfo & {
        mDevice = device;
        return *this;
    }

    auto ThumbnailRendererCreateInfo::SetAssetCachePathThumbnails( eastl::string_view path ) -> ThumbnailRendererCreateInfo & {
        mAssetCacheThumbnails = path;
        return *this;
    }

    auto ThumbnailRendererCreateInfo::SetShaderBasePath( eastl::string_view path ) -> ThumbnailRendererCreateInfo & {
        mShaderBasePath = path;
        return *this;
    }

    auto ThumbnailRendererCreateInfo::SetRenderResolution( RenderResolution resolution ) -> ThumbnailRendererCreateInfo & {
        mResolution = resolution;
        return *this;
    }

    ThumbnailRenderer::ThumbnailRenderer( const ThumbnailRendererCreateInfo& desc  )
        : mDevice{ desc.mDevice }, mAssetCacheThumbnails{ desc.mAssetCacheThumbnails }
    {

    }

    auto ThumbnailRenderer::Init() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Temporary, as the Direct3D 11 backend does not offer support for
        // bindless which the frame graph relies on for most of its functionality
        if (mDevice->IsGraphicsApi(GraphicsAPI::eD3D11) || mDevice->IsGraphicsApi( GraphicsAPI::eD3D12 )) {
            MKT_CORE_LOGGER_WARN( "Scene renderer expects Vulkan" );
            return;
        }

        const ShaderLibraryDescription description{
            .mDevice = mDevice,
            .mRootPath{ "Resources/Shaders/slang" } };
        mShaderLibrary = eastl::make_unique<ShaderLibrary>( description );

        if (mShaderLibrary) {
            mShaderLibrary->Initialize();
        }

        mFrameGraph = FrameGraph::Create( mDevice, mShaderLibrary.get() );

        InitPasses();
    }

    auto ThumbnailRenderer::Shutdown() -> void {
        if (mShaderLibrary) {
            mShaderLibrary->Shutdown();
            mShaderLibrary.reset();
        }
    }

    auto ThumbnailRenderer::Render( const scene::Scene *scene ) -> void {
        // Temporary, as the Direct3D 11 backend does not offer support for
        // bindless which the frame graph relies on for most of its functionality
        if (mDevice->IsGraphicsApi(GraphicsAPI::eD3D11) || mDevice->IsGraphicsApi( GraphicsAPI::eD3D12 )) {
            return;
        }
    }

    auto ThumbnailRenderer::GenerateThumbnail( asset::ModelHandle model, core::i32 width, core::i32 height ) -> rhi::TextureHandle {
        return TextureHandle::CreateEmpty();
    }

    auto ThumbnailRenderer::GenerateThumbnail( asset::FontHandle model, core::i32 width, core::i32 height ) -> rhi::TextureHandle {
        return TextureHandle::CreateEmpty();
    }

    auto ThumbnailRenderer::GenerateThumbnail( rhi::TextureHandle original,  rhi::TextureSubresourceSet slice ) -> rhi::TextureHandle {


        return TextureHandle::CreateEmpty();
    }

    auto ThumbnailRenderer::GenerateThumbnail( rhi::TextureHandle original, core::i32 width, core::i32 height ) -> rhi::TextureHandle {
        // From middle of original image I create a square as big as specified dimensions

        return TextureHandle::CreateEmpty();
    }

    auto ThumbnailRenderer::Create( const ThumbnailRendererCreateInfo &spec ) -> eastl::unique_ptr<ThumbnailRenderer> {
        return eastl::make_unique<ThumbnailRenderer>( spec );
    }

    auto ThumbnailRenderer::InitPasses() -> void {

    }
}// namespace Mikoto