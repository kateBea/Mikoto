//
// Created by zanet on 3/27/2025.
//

#ifndef TASKSCHEDULER_HH
#define TASKSCHEDULER_HH

#include <TaskScheduler.h>

#include <Library/Utility/Types.hh>
#include <span>

#include "Threading/Task.hh"
#include "Threading/ThreadUtility.hh"

namespace Mikoto {

    class TaskManager {
    public:
        explicit TaskManager( UInt32 numThreads = 8 )
            : m_ThreadCount( numThreads ) {
        }

        auto Init() -> void {
            m_TaskScheduler.Initialize( m_ThreadCount );
        }

        auto Shutdown() -> void {
            m_TaskScheduler.WaitforAllAndShutdown();
        }

        // Submit a single task to the task scheduler
        auto SubmitTask( ITask* task ) -> void {
            m_Task.emplace_back( task );
        }

        // total number or workers we have available right now (not that are free or sleeping)
        auto GetAvailableWorkerCount() const -> UInt32 {
            return m_TaskScheduler.GetNumTaskThreads();
        }

        // Submit a task with a specific thread index
        auto SubmitAttachedTask( IAttachedTask* task ) -> void {
            m_TaskScheduler.AddPinnedTask( task );
        }

        auto RunAttachedTasks( const std::vector<IAttachedTask*>& tasks ) -> void {
            for ( auto& item: tasks ) {
                if ( item->GetThreadNumber() >= m_TaskScheduler.GetNumTaskThreads() ) {
                    // Should instead assign to threads that do not have pending jobs

                    item->SetThreadNumber( 0 );
                }

                m_TaskScheduler.AddPinnedTask( item );
            }

            m_TaskScheduler.RunPinnedTasks();
        }

        auto EnqueueAttachedTask( const std::vector<IAttachedTask*>& tasks ) -> void {
            for ( auto& item: tasks ) {
                if ( item->GetThreadNumber() >= m_TaskScheduler.GetNumTaskThreads() ) {

                    item->SetThreadNumber( 0 );
                }

                m_TaskScheduler.AddPinnedTask( item );
            }
        }

        // Wait for all tasks to complete
        auto WaitForAllTasks() -> void {

            enki::TaskSet set{ static_cast<uint32_t>( m_Task.size() ),
                               [this]( enki::TaskSetPartition range, uint32_t threadnum ) {
                                   for ( uint32_t i = range.start; i < range.end; ++i ) {
                                       if ( !m_Task[i]->IsCompleted() ) {
                                           m_Task[i]->Execute();
                                           m_Task[i]->SetIsCompleted( true );
                                       }
                                   }
                               } };

            m_TaskScheduler.AddTaskSetToPipe( &set );

            m_TaskScheduler.WaitforAll();

            m_Task.clear();
        }

        auto GetThreadCount() const -> uint32_t { return m_ThreadCount; }

    private:
        UInt32 m_ThreadCount{};
        std::vector<ITask*> m_Task{};
        enki::TaskScheduler m_TaskScheduler{};
    };

}// namespace Mikoto


#endif//TASKSCHEDULER_HH
