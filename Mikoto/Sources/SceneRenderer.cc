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
        : mDevice{ createInfo.mDevice },
        mPresentTexture{ createInfo.mPresentTexture },
        mTargetResolution{ createInfo.mResolution } {}

    auto SceneRenderer::Init() -> void {
        // Init shader library
        const ShaderLibraryDescription description{
            .mDevice = mDevice,
            .mRootPath{ "Resources/Shaders/slang" },
        };

        mShaderLibrary = eastl::make_unique<ShaderLibrary>( description );
        if (mShaderLibrary) {
            mShaderLibrary->Initialize();
        }

        mFrameGraph = FrameGraph::Create( mDevice, mShaderLibrary.get() );

        // Scene pre-passes
        mCameraPass.RegisterPasses( *mFrameGraph );
        mGeometryManagement.RegisterPasses( *mFrameGraph );

        mRenderPrepass.RegisterPasses( *mFrameGraph );
        mShadowMapping.RegisterPasses( *mFrameGraph );

        mParticleRendering.RegisterPasses( *mFrameGraph );

        mGeometryShading.RegisterPasses( *mFrameGraph );

        mMousePickingModule.RegisterPasses( *mFrameGraph );

        // Raytracing
        mPathTracing.RegisterPasses( *mFrameGraph );
        mRayTracingPass.RegisterPasses( *mFrameGraph );

        // Post process
        mTextRendering.RegisterPasses( *mFrameGraph );
        mPostEffectsPasses.RegisterPasses( *mFrameGraph );

        // Some debug passes
        mMaterialModule.RegisterPasses( *mFrameGraph );
        mDebugPasses.RegisterPasses( *mFrameGraph );

        // Render final contents into specified images
        mPresentationModule.RegisterPasses( *mFrameGraph );
        mPresentationModule.RegisterPresentImage( *mFrameGraph, mPresentTexture );

        // Build graph
        mFrameGraph->Compile();

        // Set geometry manager
        // The geometry manager prepares the GPU side scene structures
        // and knows how to submit indexed draws or indirect draws
        mShadowMapping.SetGeometryManager( mGeometryManagement );
        mGeometryShading.SetGeometryManager( mGeometryManagement );
        mRenderPrepass.SetGeometryManager( mGeometryManagement );
        mMousePickingModule.SetGeometryManager( mGeometryManagement );

        // Execution policies
        mFrameGraph->SetExecutionPolicy( "BRDFLut", FGExecutionPolicy::eOnce );

        mFrameGraph->SetExecutionPolicy( "PrefilterPass", FGExecutionPolicy::eOnChange );
        mFrameGraph->SetExecutionPolicy( "IrradiancePass", FGExecutionPolicy::eOnChange );
        mFrameGraph->SetExecutionPolicy( "SkyboxProjection", FGExecutionPolicy::eOnChange );
    }

    auto SceneRenderer::Shutdown() -> void {
        mPresentTexture.Reset();

        mFrameGraph = nullptr;
        mCamera = nullptr;
        mDevice = nullptr;
    }

    auto SceneRenderer::Render( const Scene* scene ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        mShadowMapping.SetScene( scene );
        mRenderPrepass.SetScene( scene );
        mTextRendering.SetScene( scene );
        mGeometryManagement.SetScene( scene );

        mFrameGraph->Execute();
    }

    auto SceneRenderer::SetMainCamera( const SceneCamera *camera ) -> void {
        mCameraPass.SetCamera( camera );
        mRenderPrepass.SetCamera( camera );
        mTextRendering.SetCamera( camera );
        mShadowMapping.SetCamera( camera );
        mGeometryManagement.SetCamera( camera );
    }

    auto SceneRenderer::SetClearColor( const Color& color ) -> void {
        mGeometryShading.SetClearColor( color );
    }

    auto SceneRenderer::SetSkyboxEquirectangular( TextureHandle texture ) -> void {
        if (texture.IsEmpty()) {
            return;
        }

        FGTextureHandle handle{ mFrameGraph->ImportTexture( texture ) };
        mGeometryShading.SetEquirectangular( handle );
    }

    auto SceneRenderer::SetRenderBackground( SceneBackgroundType bg ) -> void {
        mGeometryShading.SetRenderBackground( bg );
    }

    auto SceneRenderer::GetRenderGraph() const -> const FrameGraph & {
        return *mFrameGraph;
    }

    auto SceneRenderer::GetTexture( FGTextureHandle handle ) const -> TextureHandle {
        return mFrameGraph->GetTexture( handle );
    }

    auto SceneRenderer::GetBuffer( FGBufferHandle handle ) const -> BufferHandle {
        return mFrameGraph->GetBuffer( handle );
    }

    auto SceneRenderer::Create( const SceneRendererCreateInfo &spec ) -> eastl::unique_ptr<SceneRenderer> {
        return eastl::make_unique<SceneRenderer>( spec );
    }

    auto SceneRendererCreateInfo::SetName( eastl::string_view name ) -> SceneRendererCreateInfo & {
        this->mName = name;
        return *this;
    }

    auto SceneRendererCreateInfo::SetDevice( GpuDevice *device ) -> SceneRendererCreateInfo & {
        this->mDevice = device;
        return *this;
    }

    auto SceneRendererCreateInfo::SetPresentImage( TextureHandle texture ) -> SceneRendererCreateInfo & {
        mPresentTexture = texture;
        return *this;
    }

    auto SceneRendererCreateInfo::SetRenderResolution( RenderResolution resolution ) -> SceneRendererCreateInfo & {
        mResolution = resolution;
        return *this;
    }
}// namespace Mikoto