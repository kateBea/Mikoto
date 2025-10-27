//
// Created by zanet on 3/27/2025.
//

#ifndef TASKSCHEDULER_HH
#define TASKSCHEDULER_HH
#include <functional>

#include <Common/Service.hh>
#include <Common/Singleton.hh>
#include <Library/String/String.hh>
#include <Library/Utility/Types.hh>
#include <Logging/Logger.hh>

#include <taskflow/taskflow.hpp>

#include <ankerl/unordered_dense.h>

namespace Mikoto {

    class TaskManager final : public IService, public Singleton<TaskManager> {
    public:
        explicit TaskManager(UInt32 threadCount);

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto SubmitTask(std::function<void()>&& task) -> void;

        template<typename Func, typename... Args>
        auto RunPeriodically(Size seconds, Func&& func, Args&&... args ) -> UInt32 {
            auto newTask{
                std::function<void()>{ std::bind( std::forward<Func>( func ), std::forward<Args>( args )... ) }
            };

            m_PeriodicTasksFrequency.try_emplace( m_TaskIndex, seconds );
            m_PeriodicTaskCondition.try_emplace( m_TaskIndex, true );

            AddNewTaskRunner(std::move(newTask), m_TaskIndex);

            return m_TaskIndex++;
        }

        auto DisablePeriodicTask(UInt32 index) -> void;

    private:
        auto AddNewTaskRunner(std::function<void()>&& task, UInt32 index) -> void;

        UInt32 m_ThreadCount{};
        tf::Executor m_Executor{};

        UInt32 m_TaskIndex{};
        ankerl::unordered_dense::map<UInt32, Size> m_PeriodicTasksFrequency{};
        ankerl::unordered_dense::map<UInt32, bool> m_PeriodicTaskCondition{};
    };
}// namespace Mikoto


#endif//TASKSCHEDULER_HH
