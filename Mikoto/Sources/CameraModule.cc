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

#include <Core/Profiler.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Passes/CameraModule.hh>
#include <Scene/Camera.hh>
#include <Scene/Component.hh>
#include <Scene/Scene.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::scene;

    CameraModule::CameraModule( RenderResolution resolution )
        : mResolution{ resolution }
    {}

    auto CameraModule::RegisterPasses( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        RegisterCameraSetupPass( graph );
    }

    auto CameraModule::SetCamera( const Camera *camera ) -> void {
        mCamera = camera;
    }

    auto CameraModule::RegisterCameraSetupPass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        CameraModuleInfo& info{ graph.GetOrCreate<CameraModuleInfo>() };
        auto uniformDesc{ FGBufferDescription{}
            .SetName( "CameraPass_Buffer01" )
            .SetUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kCopyDst )
            .SetSizeBytes( MKT_SIZEOF( CameraInfo ) )
            .SetHeapType( HeapType::eDeviceLocal )};

        info.mCameraData = graph.Create( uniformDesc );

        graph.RegisterPass<CameraModuleInfo>(
            "CameraInfoPass",
            FGPassType::eTransfer,
            []( FGNodeBuilder &b, CameraModuleInfo& info ) {
                b.Write( info.mCameraData, FGResourceState::eCopyDest );
            },
            [this]( CommandContext &ctx, Blackboard& b ) -> void {
                const auto& data{ b.Get<CameraModuleInfo>() };

                auto dimensions{ InferDimensions( mResolution ) };
                mCameraParameters.mProjection = mCamera->GetProjection();
                mCameraParameters.mView = mCamera->GetViewMatrix();
                mCameraParameters.mInverseProjection = glm::inverse( mCamera->GetProjection() );

                mCameraParameters.mPlaneBounds = float4{ mCamera->GetNearPlane(), mCamera->GetFarPlane(), .0f, .0f };
                mCameraParameters.mScreenDimensions = float4{ dimensions.first, dimensions.second, .0f, .0f };
                mCameraParameters.mCameraPosition = float4{ mCamera->GetPosition(), 1.0f };
                mCameraParameters.mInverseViewProjection = glm::inverse( mCameraParameters.mProjection * mCameraParameters.mView );

                ctx.CopyBuffer( data.mCameraData, mCameraParameters, 0 );
            });
    }
}
