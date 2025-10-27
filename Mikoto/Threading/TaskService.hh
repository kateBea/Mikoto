/**
 * JobSystem.hh
 * Created by kate on 9/6/23.
 * */

#ifndef MIKOTO_TASK_MANAGER_HH
#define MIKOTO_TASK_MANAGER_HH

// C++ Standard Library
#include <thread>
#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <condition_variable>

#include <ankerl/unordered_dense.h>

// Project Headers
#include "Common/Common.hh"
#include "Common/Service.hh"
#include <Library/Utility/Types.hh>

#include <Threading/TaskManager.hh>

/**
 * Implements a simple job system.
 * This subsystem is still a WIP.
 * Will not be available/operative for some time in the engine.
 * */
namespace Mikoto
{
    struct TaskServiceCreateInfo
    {
        UInt32 WorkerThreadCount{0};
    };

    class TaskService final : public IService, public Singleton<TaskService> {
    public:
        explicit TaskService( const TaskServiceCreateInfo& options );

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update( float dt ) -> void override;

        template<typename Func, typename... Args>
        auto Submit( Func&& func, Args&&... args ) -> void {
            auto newTask{
                std::function<void()>{ std::bind( std::forward<Func>( func ), std::forward<Args>( args )... ) }
            };

            m_TaskManager->SubmitTask( std::move( newTask ) );
        }

        // TODO: Review
        template<typename Func, typename... Args>
        auto RunPeriodically(Size seconds, Func&& func, Args&&... args ) -> UInt32 {
            return m_TaskManager->RunPeriodically(seconds, std::forward<Func>( func ), std::forward<Args>( args )... );
        }

        MKT_NODISCARD auto GetWorkersCount() const -> UInt32 { return m_ThreadCount; }

        ~TaskService() override = default;

    private:
        auto SetupPeriodicTaskRunner() -> void;

    private:
        UInt32 m_ThreadCount{};
        Unique<TaskManager> m_TaskManager{ nullptr };
    };
}// namespace Mikoto

#endif // MIKOTO_TASK_MANAGER_HH
