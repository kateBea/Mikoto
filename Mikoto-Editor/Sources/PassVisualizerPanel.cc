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

#include <algorithm>

#include <EASTL/sort.h>
#include <EASTL/algorithm.h>
#include <EASTL/fixed_vector.h>

#include <magic_enum/magic_enum.hpp>

#include <Core/Core.hh>
#include <Core/Profiler.hh>
#include <Core/LocalizationService.hh>

#include <ImGui/Utility.hh>

#include <Panels/PassVisualizerPanel.hh>

#include <Layers/EditorLayer.hh>

namespace mikoto::editor {
    using namespace renderer;

    PassVisualizerPanel::PassVisualizerPanel( const PassVisualizerDescription& description )
        : Panel{ "Pass Visualizer", "Pass Visualizer", description.mGroup, description.mIsToggleable },
          mEditorState{ description.mState } {
        MKT_BEGIN_PROFILER();
        mPanelIsVisible = false;
    }

    auto PassVisualizerPanel::Capture() -> void {
        MKT_BEGIN_PROFILER();
        imgui::GraphEditorBuilder builder{};
        const auto* sceneRenderer{ mEditorState ? mEditorState->mEditorRenderer : nullptr };

        if ( sceneRenderer && sceneRenderer->HasFrameGraph() ) {
            const auto& control{ sceneRenderer->GetNodeControl() };
            const auto& statistics{ sceneRenderer->GetPassStatistics() };

            for ( const auto& entry: control.mNodes ) {
                const auto& name{ entry.first };
                const auto& pass{ entry.second };

                auto& node{ builder.PushNode( name ) };

                switch ( pass.mType ) {
                    case FGPassType::eGraphics:
                        node.mType = MKT_LOC( "pass_visualizer_graphics" ).c_str();
                        break;
                    case FGPassType::eCompute:
                        node.mType = MKT_LOC( "pass_visualizer_compute" ).c_str();
                        break;
                    case FGPassType::eTransfer:
                        node.mType = MKT_LOC( "pass_visualizer_transfer" ).c_str();
                        break;
                    default:
                        node.mType = MKT_LOC( "pass_visualizer_generic" ).c_str();
                        break;
                }

                switch ( pass.mExecutionPolicy ) {
                    case FGExecutionPolicy::ePerFrame:
                        node.mPolicy = MKT_LOC( "pass_visualizer_per_frame" ).c_str();
                        break;
                    case FGExecutionPolicy::eOnWake:
                        node.mPolicy = MKT_LOC( "pass_visualizer_on_wake" ).c_str();
                        break;
                    case FGExecutionPolicy::eOnce:
                        node.mPolicy = MKT_LOC( "pass_visualizer_once" ).c_str();
                        break;
                }

                node.mEnabled = pass.mIsAlive;

                for ( const auto& dependency: pass.mDependsOn ) {
                    node.mDependencies.push_back( dependency );
                }

                if ( auto stat = statistics.find( name ); stat != statistics.end() ) {
                    node.mExecuted = stat->second.mExecutedThisFrame;
                    node.mSamples = stat->second.mExecutionCount;
                    node.mLastMs = stat->second.mLastExecutionTime;
                    node.mMinMs = stat->second.mMinExecutionTime;
                    node.mMaxMs = stat->second.mMaxExecutionTime;
                }

                eastl::fixed_vector<FGResourceHandle, 32, true> handles{ pass.mReadResources.begin(), pass.mReadResources.end() };
                handles.insert( handles.end(), pass.mWriteResources.begin(), pass.mWriteResources.end() );

                for ( const auto& resource: pass.mResourceStates ) {
                    const auto& handle{ resource.first };

                    handles.push_back( handle );
                }

                eastl::sort( handles.begin(), handles.end() );
                handles.erase( eastl::unique( handles.begin(), handles.end() ), handles.end() );

                for ( const auto handle: handles ) {
                    imgui::GraphResourceUsage usage{};
                    usage.mHandle = handle;
                    usage.mRead = eastl::find( pass.mReadResources.begin(), pass.mReadResources.end(), handle ) != pass.mReadResources.end();
                    usage.mWrite = eastl::find( pass.mWriteResources.begin(), pass.mWriteResources.end(), handle ) != pass.mWriteResources.end();

                    if ( auto track = pass.mResourceStates.find( handle ); track != pass.mResourceStates.end() ) {
                        for ( const auto pipelineStage: track->second.mStages ) {
                            if ( !usage.mStage.empty() ) {
                                usage.mStage.append( " | " );
                            }

                            const auto stage{ magic_enum::enum_name( pipelineStage ) };
                            usage.mStage.append( stage.data(), stage.size() );
                        }
                    }

                    if ( auto resource = control.mResources.find( handle ); resource != control.mResources.end() ) {
                        usage.mName = resource->second.mName;
                        usage.mImported = resource->second.mIsImported;

                        if ( const auto* texture = eastl::any_cast<FGTextureDescription>( &resource->second.mDescription ) ) {
                            const auto format{ magic_enum::enum_name( texture->mFormat ) };
                            usage.mDetails = string::Format( fmt::runtime( MKT_LOC( "pass_visualizer_texture_x_mips" ).c_str() ),
                                texture->mWidth, texture->mHeight, format, texture->mMipCount );
                        }
                        else if ( const auto* buffer = eastl::any_cast<FGBufferDescription>( &resource->second.mDescription ) ) {
                            usage.mDetails = string::Format( fmt::runtime( MKT_LOC( "pass_visualizer_buffer_bytes_elements" ).c_str() ),
                                static_cast<core::u64>( buffer->mElementCount ) * buffer->mElementSizeBytes, buffer->mElementCount );
                        }
                        else {
                            usage.mDetails = MKT_LOC( "pass_visualizer_external_resource" ).c_str();
                        }
                    }

                    if ( usage.mName.empty() ) {
                        usage.mName = string::Format( fmt::runtime( MKT_LOC( "pass_visualizer_resource" ).c_str() ), handle );
                    }

                    node.mResources.emplace_back( eastl::move( usage ) );
                }
            }
        }

        mGraph.Build( builder );
    }

    auto PassVisualizerPanel::OnRender( float ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mPanelIsVisible ) {
            return;
        }

        ImGui::SetNextWindowSize( { 1280, 740 }, ImGuiCond_FirstUseEver );

        if ( imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse ) ) {
            mPanelIsFocused = ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows );
            mPanelIsHovered = ImGui::IsWindowHovered( ImGuiHoveredFlags_RootAndChildWindows );

            if ( !mGraph.IsPaused() ) {
                Capture();
            }

            mGraph.RenderContents();
        }

        ImGui::End();
    }
}
