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

#include <ranges>

#include <EASTL/utility.h>
#include <EASTL/queue.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Exception.hh>

#include <Threading/TaskService.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/FrameGraphCompiler.hh>

namespace mikoto::renderer {

    FGCompiler::FGCompiler( rhi::IGpuDevice* device )
        : mDevice{ device } {
        if ( !mDevice ) {
            MKT_THROW_INVALID_ARGUMENT( "Frame graph compiler requires a GPU device." );
        }
    }

    auto FGCompiler::Compile( FrameGraph& graph ) -> core::Ref<FGExecutionPlan> {
        MKT_BEGIN_PROFILER();

        if ( !graph.mNodeControl ) {
            MKT_THROW_INVALID_ARGUMENT( "Frame graph compiler requires node control." );
        }

        mGraph = &graph;
        mNodeControl = graph.mNodeControl.get();
        mResources = graph.mResourceManager.get();
        mExecutionPlan = core::Ref<FGExecutionPlan>::New();

        // Resolve pass dependencies and discard work that cannot contribute to an output.
        BuildNodeEdges();
        BuildExecutionContext();
        CullGraphNodes();

        // Allocate only after the execution plan and its resource transitions are known.
        BuildNodeBarriers();
        BuildResources();

        return mExecutionPlan;
    }

    auto FGCompiler::BuildResources() -> void {
        MKT_BEGIN_PROFILER();

        struct ResourceBuild {
            FGResource* mTarget{};
            FGNodeResource* mDescription{};
            core::Ref<core::Resource> mResult{};
            tf::Task mTask{};
        };

        eastl::vector<ResourceBuild> builds{};
        builds.reserve( mNodeControl->mResources.size() );

        ankerl::unordered_dense::map<FGResourceHandle, core::usize> pending{};

        // A shader table belongs to its pipeline generation and must be rebuilt
        // along with it, even when its own description has not changed.
        for ( const auto& [handle, description]: mNodeControl->mResources ) {
            auto& resource{ mResources->Get( handle ) };

            if ( description.mIsImported || resource.mType != FGResourceType::eShaderTable ) {
                continue;
            }

            const auto& table{ eastl::any_cast<const FGShaderTableDescription&>( description.mDescription ) };
            const auto& pipeline{ mResources->Get( table.mPipeline.mHandle ) };

            if ( pipeline.mType != FGResourceType::ePipeline ) {
                MKT_THROW_INVALID_ARGUMENT( "A graph shader table must reference a pipeline." );
            }

            resource.mNeedsRebuild |= pipeline.mNeedsRebuild;
        }

        // Snapshot stable slots before starting workers. Jobs never insert into
        // either resource map or publish a partially completed GPU generation.
        for ( auto& [handle, description]: mNodeControl->mResources ) {
            auto& resource{ mResources->Get( handle ) };

            if ( description.mIsImported || !resource.mNeedsRebuild ) {
                continue;
            }

            pending.emplace( handle, builds.size() );
            builds.push_back( ResourceBuild{ .mTarget = &resource, .mDescription = &description } );
        }

        if ( builds.empty() ) {
            return;
        }

        const auto create{ [this, &builds, &pending]( core::usize index ) {
            auto& build{ builds[index] };

            if ( build.mTarget->mType == FGResourceType::eAccelerationStructure ) {
                // Its description only reserves a slot. Geometry-dependent
                // BLAS/TLAS creation and GPU builds remain in the upload pass.
                return;
            }

            if ( build.mTarget->mType == FGResourceType::eShaderTable ) {
                const auto& table{ eastl::any_cast<const FGShaderTableDescription&>( build.mDescription->mDescription ) };
                const auto pipelineBuild{ pending.find( table.mPipeline.mHandle ) };
                const rhi::PipelineHandle pipeline{ pipelineBuild == pending.end()
                                                        ? mResources->Get( table.mPipeline.mHandle ).mResource
                                                        : builds[pipelineBuild->second].mResult };
                build.mResult = mGraph->CreateResource( build.mTarget->mType, *build.mDescription, pipeline );
            }
            else {
                build.mResult = mGraph->CreateResource( build.mTarget->mType, *build.mDescription );
            }
        } };

        tf::Taskflow flow{ "FrameGraph resources" };

        for ( core::usize index{}; index < builds.size(); ++index ) {
            builds[index].mTask = flow
            .emplace( [create, index] {
                create( index );
            } )
            .name( builds[index].mDescription->mName.c_str() );
        }

        for ( auto& build: builds ) {
            if ( build.mTarget->mType != FGResourceType::eShaderTable ) {
                continue;
            }

            const auto& table{ eastl::any_cast<const FGShaderTableDescription&>( build.mDescription->mDescription ) };

            if ( const auto pipeline{ pending.find( table.mPipeline.mHandle ) }; pipeline != pending.end() ) {
                builds[pipeline->second].mTask.precede( build.mTask );
            }
        }

        auto* tasks{ threading::TaskService::GetPtr() };

        if ( tasks && tasks->IsInitialized() ) {
            tasks->ExecuteTaskGraph( flow, true );
        }

        // Submit(..., true) joins all jobs and propagates failures. Publish only
        // after the entire batch succeeds; failures leave dirty slots retryable.
        for ( auto& build: builds ) {
            if ( build.mTarget->mType == FGResourceType::eAccelerationStructure ) {
                build.mTarget->mNeedsRebuild = false;
            }
            else {
                mResources->SetResource( build.mTarget->mResourceID, eastl::move( build.mResult ) );
            }
        }

        // Workers have joined and every resource has been published. Keep the
        // creation settings, but release one-shot CPU data unless retention was requested.
        for ( auto& build: builds ) {
            auto& node{ *build.mDescription };

            if ( build.mTarget->mType == FGResourceType::eTexture ) {
                auto& texture{ eastl::any_cast<rhi::TextureCreateDescription&>( node.mDescription ) };

                if ( !texture.mKeepInitializerResources ) {
                    node.mInitializersReleased |= !texture.mImageHandle.IsEmpty() || !texture.mBufferSpan.IsEmpty();
                    texture.mImageHandle.Reset();
                    texture.mBufferSpan.Reset();
                }
            }
            else if ( build.mTarget->mType == FGResourceType::eBuffer ) {
                auto& buffer{ eastl::any_cast<rhi::BufferCreateDescription&>( node.mDescription ) };

                if ( !buffer.mKeepInitializerResources ) {
                    node.mInitializersReleased |= !buffer.mSpanHandle.IsEmpty();
                    buffer.mSpanHandle.Reset();
                }
            }
        }
    }

    auto FGCompiler::BuildNodeEdges() -> void {
        MKT_BEGIN_PROFILER();
        // Compilation may be repeated after editing the graph.
        for ( auto& [name, pass]: mNodeControl->mNodes ) {
            pass.mSuccessors.clear();
            pass.mInDegree = 0;
        }

        for ( auto& [name, pass]: mNodeControl->mNodes ) {
            for ( const auto& dependency: pass.mDependsOn ) {
                auto producer{ mNodeControl->mNodes.find( dependency ) };

                if ( producer == mNodeControl->mNodes.end() ) {
                    MKT_THROW_RUNTIME_ERROR( "Frame graph pass references a missing dependency." );
                }

                producer->second.mSuccessors.push_back( pass.mName );
                ++pass.mInDegree;
            }
        }
    }

    auto FGCompiler::BuildExecutionContext() -> void {
        MKT_BEGIN_PROFILER();

        // TopoSort Kahn's Topological Sort
        eastl::queue<eastl::string> q{};
        ankerl::unordered_dense::map<eastl::string, core::u32> inDegrees{};
        auto& passesMap{ mNodeControl->mNodes };

        for ( const auto& pass: passesMap | std::views::values ) {
            inDegrees[pass.mName] = passesMap.at( pass.mName ).mInDegree;

            if ( inDegrees[pass.mName] == 0 ) {
                q.push( pass.mName );// no dependencies -> ready immediately
            }
        }

        while ( !q.empty() ) {
            eastl::string cur{ q.front() };
            q.pop();

            mExecutionPlan->mSortedPasses.push_back( cur );

            for ( const auto& succ: passesMap.at( cur ).mSuccessors ) {
                if ( --inDegrees[succ] == 0 ) {// all successors dependencies are done?
                    q.push( succ );            // successor is now ready
                }
            }
        }

        // If we didn't visit every pass, the graph has a cycle, invalid.
        if ( mExecutionPlan->mSortedPasses.size() != passesMap.size() ) {
            MKT_THROW_RUNTIME_ERROR( "Frame graph contains a dependency cycle." );
        }
    }

    auto FGCompiler::CullGraphNodes() -> void {
        MKT_BEGIN_PROFILER();
        auto& passesMap{ mNodeControl->mNodes };

        // NOTE: No proper Dead Stripping (remove nodes that do not contribute to final image)
        // is done yet cause I usually have nodes I wanna see they output for debug purposes

        // Cull Passes
        // This step is done if we for instance remove or disable a pass
        // we need to disable the ones depending on it?
        if ( !mExecutionPlan->mSortedPasses.empty() ) {
            for ( auto reverseIt{ mExecutionPlan->mSortedPasses.rbegin() }; reverseIt != mExecutionPlan->mSortedPasses.rend(); ++reverseIt ) {
                if ( !passesMap[*reverseIt].mIsAlive ) {
                    continue;// skip dead passes
                }

                // Commenting this works for now because all frame graph resources
                // are persistent across frames so I can disable a node and not
                // have to worry about resource lifetime if needed somewhere else
                // for (const auto& pass: passesMap[*reverseIt].mDependsOn) {
                //     passesMap[pass].mIsAlive = true; // my dependency is needed -> keep it alive
                // }
            }
        }
    }

    auto FGCompiler::BuildNodeBarriers() -> void {
        MKT_BEGIN_PROFILER();

        for ( const auto& passName: mExecutionPlan->mSortedPasses ) {
            const auto& pass{ mNodeControl->mNodes.at( passName ) };
            auto& barriers{ mExecutionPlan->mBarriers[passName] };

            // Keep every declared use, including currently disabled passes.
            // EnablePass/DisablePass and Once/OnWake policies can change the
            // actual producer without recompiling this plan. Resolve source
            // states when recording; compilation must not advance resource state.
            for ( const auto& [resourceHandle, usage]: pass.mResourceStates ) {
                const auto& resource{ mNodeControl->mResources.at( resourceHandle ) };
                barriers.emplace( resourceHandle, eastl::make_pair(
                resource.mName, FGBarrier{
                    .mResourceID = resourceHandle,
                    .mNewStages = usage.mStages,
                    .mNewAccess = usage.mAccess } ) );
            }
        }
    }
}// namespace mikoto::renderer