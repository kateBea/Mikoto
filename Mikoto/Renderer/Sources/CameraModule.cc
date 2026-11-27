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
#include <Core/Profiler.hh>
#include <Core/Types.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/AtmosphericScatteringModule.hh>
#include <Renderer/ForwardRenderer/Passes/CameraModule.hh>
#include <Renderer/ForwardRenderer/Passes/CloudsModule.hh>
#include <Renderer/ForwardRenderer/Passes/VolumeModule.hh>

#include <Scene/Camera.hh>
#include <Scene/Component.hh>
#include <Scene/Scene.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::renderer::rhi;

    CameraModule::CameraModule( renderer::rhi::RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto CameraModule::SetExposure( core::f32 exposure ) -> void {
        MKT_BEGIN_PROFILER();
        mExposure = exposure;
    }

    auto CameraModule::SetGamma( float value ) -> void {
        MKT_BEGIN_PROFILER();
        mGamma = value;
    }

    auto CameraModule::SetCamera( const scene::Camera *camera ) -> void {
        MKT_BEGIN_PROFILER();
        mCamera = camera;
    }

    auto CameraModule::RegisterCameraSetupPass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto &info{ graph.GetOrCreate<CameraModuleInfo>() };
        auto bufferDesc{ FGBufferDescription{}
            .SetName( "CameraPass_Buffer01" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured | renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( MKT_SIZEOF( CameraData ) )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) };
        info.mCameraData = graph.Create( bufferDesc );

        graph.RegisterPass(
            "CameraInfoPass",
            FGPassType::eTransfer,
            []( FGNodeBuilder &builder, Blackboard &b ) {
                builder.UseResource( b.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                if ( const auto *clouds{ b.TryGet<CloudsModuleInfo>() } ) {
                    builder.UseResource( clouds->mFrame, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                }
                if ( const auto *volumes{ b.TryGet<VolumeModuleInfo>() } ) {
                    builder.UseResource( volumes->mData, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                }
                if ( const auto *fog{ b.TryGet<FogModuleInfo>() } ) {
                    builder.UseResource( fog->mFrame, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                }
            },
            [this]( CommandContext &ctx, core::Blackboard &b ) -> void {
                const auto &data{ b.Get<CameraModuleInfo>() };

                auto dimensions{ InferDimensions( mResolution ) };
                mCameraData.mProjection = mCamera->GetProjection();
                mCameraData.mView = mCamera->GetViewMatrix();
                mCameraData.mInverseProjection = glm::inverse( mCamera->GetProjection() );

                mCameraData.mPlaneBounds = core::float4{ mCamera->GetNearPlane(), mCamera->GetFarPlane(), .0f, .0f };
                mCameraData.mScreenDimensions = core::float4{ dimensions.first, dimensions.second, .0f, .0f };
                mCameraData.mCameraPosition = core::float4{ mCamera->GetPosition(), 1.0f };
                mCameraData.mInverseViewProjection = glm::inverse( mCameraData.mProjection * mCameraData.mView );

                mCameraData.mGamma = mGamma;
                mCameraData.mExposure = mExposure;
                mCameraData.mVolumes = b.Contains<VolumeModuleInfo>() ? ctx.GetDeviceBufferAddress( b.Get<VolumeModuleInfo>().mData ) : 0;
                mCameraData.mFogFrame = b.Contains<FogModuleInfo>() ? ctx.GetDeviceBufferAddress( b.Get<FogModuleInfo>().mFrame ) : 0;

                mCameraData.mClouds = b.Contains<CloudsModuleInfo>() ? ctx.GetDeviceBufferAddress( b.Get<CloudsModuleInfo>().mFrame ) : 0;
                mCameraData.mAtmosphere = 0;
                if ( const auto *atmosphere{ b.TryGet<AtmosphereModuleInfo>() }; atmosphere && atmosphere->mSunLight ) {
                    // Surface passes consume these bindings after the LUT bake.
                    mCameraData.mAtmosphere = ctx.GetDeviceBufferAddress( atmosphere->mParameters );
                    mCameraData.mAtmosphereTransmittance = ctx.PushTexture_SRV( atmosphere->mTransmittance );
                    mCameraData.mAtmosphereSampler = ctx.PushSampler( atmosphere->mSampler );
                }

                ctx.CopyBuffer( data.mCameraData, mCameraData, 0 );
            } );
    }

    auto CameraModule::SetVisualizeCascades( bool enabled ) -> void {
        mCameraData.mVisualizeCascades = enabled ? MKT_SHADER_TRUE : MKT_SHADER_FALSE;
    }
}// namespace mikoto::renderer
