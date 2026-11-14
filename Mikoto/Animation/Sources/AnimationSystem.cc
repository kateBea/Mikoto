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


#include <EASTL/algorithm.h>

#include <Core/Core.hh>
#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Threading/TaskService.hh>

#include <Animation/AnimationSystem.hh>

namespace mikoto::animation {

    AnimationSystem::AnimationSystem( const AnimationSystemCreateInfo& ) {
    }

    auto AnimationSystem::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        MKT_CORE_LOGGER_INFO( "Initializing AnimationSystem..." );
        mIsInitialized = true;
    }

    auto AnimationSystem::Shutdown() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIsInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Shutting down AnimationSystem..." );

        mAnimators.clear();
        mUpdateList.clear();
        mUpdateTasks.clear();
        mUpdateTaskCount = 0;
        mIsInitialized = false;
    }

    auto AnimationSystem::Update( core::f32 dt ) -> void {
        MKT_BEGIN_PROFILER();
        mUpdateList.clear();

        for ( auto& [id, animator]: mAnimators ) {
            if ( !animator->IsSuspended() ) {
                mUpdateList.push_back( animator.get() );
            }
        }

        auto* pTaskService{ threading::TaskService::GetPtr() };
        MKT_ASSERT( pTaskService, "The task service was not initialized." );

        // Keep small populations inline; task scheduling should not cost more
        // than sampling. Each worker claims batches to balance differently sized rigs.
        constexpr core::usize grain{ 4 };
        const core::usize workers{ pTaskService && pTaskService->IsInitialized() ? pTaskService->GetWorkersCount() : 1 };
        const core::usize taskCount{ eastl::min( workers, mUpdateList.size() / grain ) };

        if ( taskCount < 2 ) {
            for ( auto* animator: mUpdateList ) {
                animator->Update( dt );
            }

            return;
        }

        mUpdateDelta = dt;
        mNextUpdate.store( 0, std::memory_order_relaxed );

        if ( taskCount != mUpdateTaskCount ) {
            mUpdateTasks.clear();

            for ( core::usize task{}; task < taskCount; ++task ) {
                mUpdateTasks.emplace( [this] {
                    for ( ;; ) {
                        const auto first{ mNextUpdate.fetch_add( grain, std::memory_order_relaxed ) };

                        if ( first >= mUpdateList.size() ) {
                            return;
                        }

                        const auto end{ eastl::min( first + grain, mUpdateList.size() ) };

                        for ( core::usize index{ first }; index < end; ++index ) {
                            mUpdateList[index]->Update( mUpdateDelta );
                        }
                    }
                } );
            }

            mUpdateTaskCount = taskCount;
        }

        // As in Ozz's samples/multithread, contexts and output poses are private
        // per character. Joining publishes complete poses to every render consumer.
        pTaskService->ExecuteTaskGraph( mUpdateTasks, true );
    }

    auto AnimationSystem::RegisterAnimation( asset::ModelHandle handle ) -> core::u32 {
        MKT_BEGIN_PROFILER();

        if ( handle.IsEmpty() || !handle->HasArmature() ) {
            return 0;
        }

        core::u32 animatorID{ mNextAnimatorID++ };
        mAnimators.try_emplace( animatorID, eastl::make_unique<Animator>( handle ) );

        return animatorID;
    }

    auto AnimationSystem::GetAnimator( core::u32 id ) -> Animator* {
        MKT_BEGIN_PROFILER();
        const auto it{ mAnimators.find( id ) };

        if ( it != mAnimators.end() ) {
            return it->second.get();
        }

        return nullptr;
    }

    auto AnimationSystem::UnregisterAnimation( core::u32 id ) -> void {
        MKT_BEGIN_PROFILER();
        mAnimators.erase( id );
    }
}// namespace mikoto::animation
