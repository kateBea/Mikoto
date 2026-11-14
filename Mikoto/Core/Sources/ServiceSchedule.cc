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
#include <EASTL/algorithm.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>
#include <Core/ServiceSchedule.hh>

namespace mikoto::core {

    auto ServiceSchedule::Build( eastl::span<const ServiceScheduleNode> nodes, eastl::function<void( usize )> callback, cstr name ) -> void {
        MKT_BEGIN_PROFILER();
        Clear();
        mCallback = eastl::move( callback );

        eastl::vector<usize> pending( nodes.size(), 0 );
        eastl::vector<eastl::vector<usize>> successors( nodes.size() );
        eastl::vector<usize> mainReady{};
        eastl::vector<usize> workerReady{};

        for ( usize index{}; index < nodes.size(); ++index ) {
            const auto& node{ nodes[index] };

            for ( usize dependency: node.mDependencies ) {
                if ( dependency >= nodes.size() ) {
                    MKT_THROW_RUNTIME_ERROR( "Service schedule contains an invalid dependency index" );
                }
                // Count each edge once, including declarations combined by callers.
                auto& children{ successors[dependency] };

                if ( eastl::find( children.begin(), children.end(), index ) == children.end() ) {
                    children.push_back( index );
                    ++pending[index];
                }
            }
        }

        auto makeReady{ [&]( usize index ) {
            if ( nodes[index].mMainThread ) {
                mainReady.push_back( index );
            }
            else {
                workerReady.push_back( index );
            }
        } };

        for ( usize index{}; index < nodes.size(); ++index ) {
            if ( pending[index] == 0 ) {
                makeReady( index );
            }
        }

        usize visited{};
        auto releaseSuccessors{ [&]( usize index ) {
            ++visited;

            for ( usize successor: successors[index] ) {
                if ( --pending[successor] == 0 ) {
                    makeReady( successor );
                }
            }
        } };

        // Kahn's topological traversal detects cycles before any callback runs.
        // Exhaust a ready worker chain into ONE DAG, not one barrier per level:
        // A -> B can advance while an unrelated C is still running. Only a
        // main-thread boundary joins the batch, so workers never block waiting
        // for the application thread (which would deadlock a small thread pool).
        eastl::vector<tf::Task> tasks( nodes.size() );
        eastl::vector<usize> batchIndices( nodes.size(), nodes.size() );
        usize mainCursor{};
        usize workerCursor{};

        while ( mainCursor < mainReady.size() || workerCursor < workerReady.size() ) {
            if ( mainCursor < mainReady.size() ) {
                const usize index{ mainReady[mainCursor++] };
                mBatches.push_back( Batch{ .mMainThreadNode = index } );
                releaseSuccessors( index );
                continue;
            }

            const usize batchIndex{ mBatches.size() };
            const usize firstWorker{ workerCursor };
            auto flow{ eastl::make_unique<tf::Taskflow>() };
            flow->name( name );

            while ( workerCursor < workerReady.size() ) {
                const usize index{ workerReady[workerCursor++] };
                tasks[index] = flow->emplace( [this, index]() {
                    mCallback( index );
                } ).name( nodes[index].mName );
                batchIndices[index] = batchIndex;
                releaseSuccessors( index );
            }

            for ( usize cursor{ firstWorker }; cursor < workerCursor; ++cursor ) {
                const usize index{ workerReady[cursor] };

                for ( usize successor: successors[index] ) {
                    if ( batchIndices[successor] == batchIndex ) {
                        tasks[index].precede( tasks[successor] );
                    }
                }
            }

            mBatches.push_back( Batch{ .mWorkers = eastl::move( flow ) } );
        }

        if ( visited != nodes.size() ) {
            eastl::string message{ "Cycle in " };
            message += name;
            message += "; blocked services: ";

            for ( usize index{}; index < nodes.size(); ++index ) {
                if ( pending[index] != 0 ) {
                    message += nodes[index].mName;
                    message += "; ";
                }
            }

            Clear();
            MKT_THROW_RUNTIME_ERROR( message.c_str() );
        }
    }

    auto ServiceSchedule::Run( tf::Executor& executor ) -> void {
        MKT_BEGIN_PROFILER();

        for ( auto& batch: mBatches ) {
            if ( batch.mWorkers ) {
                // get(), unlike wait(), also propagates the task exception.
                executor.run( *batch.mWorkers ).get();
            }
            else {
                mCallback( batch.mMainThreadNode );
            }
        }
    }

    auto ServiceSchedule::Clear() -> void {
        mBatches.clear();
        mCallback = {};
    }
}// namespace mikoto::core
