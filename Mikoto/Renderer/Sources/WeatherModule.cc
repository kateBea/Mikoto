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
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/ForwardRenderer/Passes/WeatherModule.hh>
#include <Renderer/Rhi/Types.hh>

#include <Scene/Camera.hh>
#include <Scene/Scene.hh>

namespace mikoto::renderer {

    WeatherModule::WeatherModule( rhi::RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto WeatherModule::RegisterWindSimulation( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER();

        const auto field{ graph.Create( FGBufferDescription{}
            .SetName( "Wind_Field" )
            .SetSizeBytes( 16 + scene::kMaxWindAreas * sizeof( scene::WindAreaData ) )
            .SetHeapType( rhi::HeapType::eDeviceLocal )
            .SetUsage( rhi::BufferUsageFlagsBits::Structured |
                           rhi::BufferUsageFlagsBits::CopyDest |
                           rhi::BufferUsageFlagsBits::CopySource ) ) };

        graph.GetOrCreate<WeatherModuleInfo>().mWindField = field;

        graph.RegisterPass(
            "Wind_UploadField",
            FGPassType::eTransfer,
            [field]( FGNodeBuilder &builder, core::Blackboard & ) {
                builder.UseResource( field, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this, field]( CommandContext &ctx, core::Blackboard & ) {
                const auto wind{ mScene ? mScene->GetWindField() : scene::WindField{} };
                struct Header {
                    core::f32 mTime{};
                    core::u32 mCount{};
                    core::u32 mReserved[2]{};
                } header{ wind.mTime, core::as<core::u32>( wind.mAreas.size() ) };

                ctx.CopyBuffer( field, 0, &header, sizeof( header ) );

                if ( !wind.mAreas.empty() ) {
                    ctx.CopyBuffer( field, sizeof( header ), wind.mAreas.data(), wind.mAreas.size() * sizeof( scene::WindAreaData ) );
                }
            } );
    }

    auto WeatherModule::SetScene( const scene::Scene *scene ) -> void {
        mScene = scene;
    }

    auto WeatherModule::SetCamera( const scene::Camera *camera ) -> void {
        mCamera = camera;
    }
}// namespace mikoto::renderer