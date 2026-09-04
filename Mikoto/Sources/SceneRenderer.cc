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

#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Profiler.hh>
#include <Core/String.hh>
#include <Core/Types.hh>

#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/SceneRenderer.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::material;
    using namespace mikoto::renderer::rhi;

    SceneRenderer::SceneRenderer( const SceneRendererCreateInfo &createInfo )
        : mDevice{ createInfo.mDevice },
        mTargetResolution{ createInfo.mResolution } {}

    auto SceneRenderer::Init() -> void {
        const ShaderLibraryDescription description{
            .mDevice = mDevice,
            .mRootPath{ "Resources/Shaders/slang" } };
        mShaderLibrary = eastl::make_unique<ShaderLibrary>( description );

        if (mShaderLibrary) {
            mShaderLibrary->Initialize();
        }

        // Temporary, as the Direct3D 11 backend does not offer support for
        // bindless which the frame graph relies on for most of its functionality
        if ( mDevice->IsGraphicsApi( GraphicsAPI::eD3D11 ) || mDevice->IsGraphicsApi( GraphicsAPI::eD3D12 ) ) {
            MKT_CORE_LOGGER_WARN( "Scene renderer expects Vulkan" );
            return;
        }

        mFrameGraph = FrameGraph::Create( mDevice, mShaderLibrary.get() );

        // Some debug passes
        mMaterialModule.RegisterPasses( *mFrameGraph );
        mDebugPasses.RegisterPasses( *mFrameGraph );

        // Scene pre-passes
        mCameraPass.RegisterPasses( *mFrameGraph );
        mGeometryManagement.RegisterPasses( *mFrameGraph );
        mSimulationsModule.RegisterPasses( *mFrameGraph );
        mRenderPrepass.RegisterPasses( *mFrameGraph );
        mShadowMapping.RegisterPasses( *mFrameGraph );
        mParticleRendering.RegisterPasses( *mFrameGraph );
        mMousePickingModule.RegisterPasses( *mFrameGraph );

        // Shading
        mGeometryShading.RegisterPasses( *mFrameGraph );

        // Debug overlay
        mDebugOverlayModule.RegisterPasses( *mFrameGraph );

        // Raytracing
        mPathTracing.RegisterPasses( *mFrameGraph );
        mRayTracingPass.RegisterPasses( *mFrameGraph );

        // Post process
        mTextRendering.RegisterPasses( *mFrameGraph );
        mPostEffectsPasses.RegisterPasses( *mFrameGraph );
        mTonemapModule.RegisterPasses( *mFrameGraph );
        mDisplayEffectsModule.RegisterPasses( *mFrameGraph );

        // I am not sure about this pass, this one was designed to
        // ideally serve as helper for passes that required image blit-ting
        // which could be achieved with compute shaders for instance
        mHelperModule.RegisterPasses( *mFrameGraph );

        // Render final contents into specified images
        mPresentationModule.RegisterPasses( *mFrameGraph );

        // Build graph
        mFrameGraph->Compile();

        // Set geometry manager
        // The geometry manager prepares the GPU side scene structures
        // and knows how to submit indexed draws or indirect draws
        mShadowMapping.SetGeometryManager( mGeometryManagement );
        mGeometryShading.SetGeometryManager( mGeometryManagement );
        mRenderPrepass.SetGeometryManager( mGeometryManagement );
        mDebugOverlayModule.SetGeometryManager( mGeometryManagement );
        mSimulationsModule.SetGeometryManager( mGeometryManagement );
        mMousePickingModule.SetGeometryManager( mGeometryManagement );
        mDisplayEffectsModule.SetGeometryManager( mGeometryManagement );
        mIndirectLightingModule.SetGeometryManager( mGeometryManagement );

        // Execution policies
        mFrameGraph->SetExecutionPolicy( "BRDFLut", FGExecutionPolicy::eOnce );

        mFrameGraph->SetExecutionPolicy( "PrefilterPass", FGExecutionPolicy::eOnWake );
        mFrameGraph->SetExecutionPolicy( "IrradiancePass", FGExecutionPolicy::eOnWake );
        mFrameGraph->SetExecutionPolicy( "SkyboxProjection", FGExecutionPolicy::eOnWake );
        mFrameGraph->SetExecutionPolicy( "SkyboxProjection_Compute", FGExecutionPolicy::eOnWake );
        mFrameGraph->SetExecutionPolicy( "SkyboxProjection_Transfer", FGExecutionPolicy::eOnWake );
        mFrameGraph->SetExecutionPolicy( "SkyboxProjection_Graphics", FGExecutionPolicy::eOnWake );

        //For some reason this makes subsequent passes to render in Wireframe mode
        //mFrameGraph->DisablePass( "WireframePass" );
    }

    auto SceneRenderer::Shutdown() -> void {
        mFrameGraph = nullptr;
        mDevice = nullptr;

        if (mShaderLibrary) {
            mShaderLibrary->Shutdown();
            mShaderLibrary.reset();
        }
    }

    auto SceneRenderer::Render( const Scene* scene ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Temporary, as the Direct3D 11 backend does not offer support for
        // bindless which the frame graph relies on for most of its functionality
        if ( mDevice->IsGraphicsApi( GraphicsAPI::eD3D11 ) || mDevice->IsGraphicsApi( GraphicsAPI::eD3D12 ) ) {
            return;
        }

        if (!mFrameGraph) {
            return;
        }

        mShadowMapping.SetScene( scene );
        mRenderPrepass.SetScene( scene );
        mTextRendering.SetScene( scene );
        mDebugOverlayModule.SetScene( scene );
        mSimulationsModule.SetScene( scene );
        mPostEffectsPasses.SetScene( scene );
        mGeometryManagement.SetScene( scene );
        mDisplayEffectsModule.SetScene( scene );
        mIndirectLightingModule.SetScene( scene );

        mFrameGraph->Execute();
        mFrameGraph->ExecuteReadbacks();
    }

    auto SceneRenderer::GetFinalImage( FinalImageType type ) -> TextureHandle {
        if (!mFrameGraph) {
            return TextureHandle::CreateEmpty();
        }

        if (type == FinalImageType::eWireframe ) {
            mFrameGraph->EnablePass( "WireframePass" );
        } else {
            mFrameGraph->DisablePass( "WireframePass" );
        }

        // This is a deferred operation, we can specify the final image output
        // but it is not until the next call to execute that we render it
        mPresentationModule.SetFinalImageTarget( type );
        return mPresentationModule.GetFinalImage( *mFrameGraph );
    }

    auto SceneRenderer::SetMainCamera( const SceneCamera *camera ) -> void {
        mCameraPass.SetCamera( camera );
        mRenderPrepass.SetCamera( camera );
        mTextRendering.SetCamera( camera );
        mShadowMapping.SetCamera( camera );
        mDebugOverlayModule.SetCamera( camera );
        mSimulationsModule.SetCamera( camera );
        mPostEffectsPasses.SetCamera( camera );
        mGeometryManagement.SetCamera( camera );
        mDisplayEffectsModule.SetCamera( camera );
    }

    auto SceneRenderer::SetClearColor( const Color& color ) -> void {
        mGeometryShading.SetClearColor( color );
    }

    auto SceneRenderer::SetTonemapType( ToneMappingType type ) -> void {
        mTonemapModule.SetToneMapping( type );
    }

    auto SceneRenderer::SetGamma( f32 gamma ) -> void {
        mGeometryShading.SetGamma( gamma );
        mCameraPass.SetGamma( gamma );
        mPostEffectsPasses.SetGamma( gamma );
    }

    auto SceneRenderer::SetExposure( f32 exposure ) -> void {
        mGeometryShading.SetExposure( exposure );
        mCameraPass.SetExposure( exposure );
        mPostEffectsPasses.SetExposure( exposure );
    }

    auto SceneRenderer::SetSkyboxBlur( core::f32 blur ) -> void {
        mGeometryShading.SetSkyboxBlur( blur );
    }

    auto SceneRenderer::SetAmbientScale( f32 ambient ) -> void {
        mGeometryShading.SetAmbientScale( ambient );
    }

    auto SceneRenderer::SetEnablePolygonComplexity( bool value ) -> void {
        if (!mFrameGraph || !mFrameGraph->IsPassPresent( "WireframePass" )) {
            return;
        }

        if (value) {
            mFrameGraph->EnablePass( "WireframePass" );
        } else {
            mFrameGraph->DisablePass( "WireframePass" );
        }

        mGeometryShading.SetMergeWireframeToFinalOutput( value );
    }

    auto SceneRenderer::SetSkyboxMaterial( MaterialHandle material ) -> void {
        if (material.IsEmpty() || !mFrameGraph) {
            return;
        }

        SkyboxMaterial* pMaterial{ checked_cast<SkyboxMaterial*>( material.GetRaw() ) };
        if (!pMaterial->HasRequiredTextures()) {
            return;
        }

        mGeometryShading.SetSkyboxMaterial( material );

        // These passes sleep after every run,
        // we need to re-enable it again so it runs once again
        // on the next call to execute
        mFrameGraph->EnablePass( "PrefilterPass" );
        mFrameGraph->EnablePass( "IrradiancePass" );
        mFrameGraph->EnablePass( "SkyboxProjection" );

        mFrameGraph->EnablePass( "SkyboxProjection_Compute" );
        mFrameGraph->EnablePass( "SkyboxProjection_Transfer" );
        mFrameGraph->EnablePass( "SkyboxProjection_Graphics" );
    }

    auto SceneRenderer::SetRenderBackground( SceneBackgroundType bg ) -> void {
        mGeometryShading.SetRenderBackground( bg );
    }

    auto SceneRenderer::SetMultisampling( rhi::Multisampling multisampling ) -> void {
        // This might trigger a graph recompile, pending to implement graph recompile,
        // should ideally just change what is needed.
        mMultisampling = multisampling;
    }

    auto SceneRenderer::SetRenderResolution( rhi::RenderResolution resolution ) -> void {
        mTargetResolution = resolution;
    }

    auto SceneRenderer::SetEnableInfiniteGrid( bool value ) -> void {
        if (!mFrameGraph || !mFrameGraph->IsPassPresent( "InfiniteGrid" )) {
            return;
        }

        if (value) {
            mFrameGraph->EnablePass( "InfiniteGrid" );
        } else {
            mFrameGraph->DisablePass( "InfiniteGrid" );
        }
    }

    auto SceneRenderer::DisablePass( eastl::string_view passName ) -> void {
        if (!mFrameGraph) {
            return;
        }

        mFrameGraph->DisablePass( passName );
    }

    auto SceneRenderer::EnablePass( eastl::string_view passName ) -> void {
        if (!mFrameGraph) {
            return;
        }

        mFrameGraph->EnablePass( passName );
    }

    auto SceneRenderer::ReadPixel( u32 x, u32 y) const -> u32 {
        return mMousePickingModule.ReadPixel( x, y );
    }

    auto SceneRenderer::ReadPixel( const ReadPixelViewportInfo &ínfo ) const -> core::u32 {
        return mMousePickingModule.ReadPixel( ínfo );
    }

    auto SceneRenderer::GetNodeControl() const -> const FGNodeControl & {
        return mFrameGraph->GetNodeControl();
    }

    auto SceneRenderer::GetTexture( FGTextureHandle handle ) const -> TextureHandle {
        if (!mFrameGraph) {
            return TextureHandle::CreateEmpty();
        }

        return mFrameGraph->GetTexture( handle );
    }

    auto SceneRenderer::GetBuffer( FGBufferHandle handle ) const -> BufferHandle {
        if (!mFrameGraph) {
            return BufferHandle::CreateEmpty();
        }

        return mFrameGraph->GetBuffer( handle );
    }

    auto SceneRenderer::GetPassList() const -> const ankerl::unordered_dense::map<eastl::string, FGNode>& {
        return mFrameGraph->GetNodeControl().mNodes;
    }

    auto SceneRenderer::GetPassStatistics() const -> const ankerl::unordered_dense::map<eastl::string, FGNodeStatistics>& {
        return mFrameGraph->GetStatisticsManager().GetStatistics();
    }

    auto SceneRenderer::Create( const SceneRendererCreateInfo &spec ) -> eastl::unique_ptr<SceneRenderer> {
        return eastl::make_unique<SceneRenderer>( spec );
    }

    auto SceneRendererCreateInfo::SetName( eastl::string_view name ) -> SceneRendererCreateInfo & {
        this->mName = name;
        return *this;
    }

    auto SceneRendererCreateInfo::SetDevice( IGpuDevice *device ) -> SceneRendererCreateInfo & {
        this->mDevice = device;
        return *this;
    }
    auto SceneRendererCreateInfo::SetMultisampling( rhi::Multisampling multisampling ) -> SceneRendererCreateInfo & {
        mMultisampling = multisampling;
        return *this;
    }

    auto SceneRendererCreateInfo::SetRenderResolution( RenderResolution resolution ) -> SceneRendererCreateInfo & {
        mResolution = resolution;
        return *this;
    }
}// namespace Mikoto