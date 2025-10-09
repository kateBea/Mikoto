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

namespace Mikoto {

    class TaskManager final : public IService, public Singleton<TaskManager> {
    public:
        explicit TaskManager(UInt32 threadCount);

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto SubmitTask(std::function<void()>&& task) -> void;

    private:
        UInt32 m_ThreadCount{};

        tf::Executor m_Executor{};
    };
}// namespace Mikoto


#endif//TASKSCHEDULER_HH
