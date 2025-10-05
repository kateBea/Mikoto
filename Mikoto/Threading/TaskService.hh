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

// Project Headers
#include "Common/Common.hh"
#include "Common/Service.hh"
#include <Library/Utility/Types.hh>

#include <Threading/Task.hh>
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

    class TaskService final : public IService, public Singleton<TaskService>
    {
    public:
        explicit TaskService(const TaskServiceCreateInfo& options);

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update(float dt) -> void override;

        auto GetManager() const -> TaskManager* { return m_TaskManager.get(); }
        auto GetWorkersCount() const -> UInt32 { return m_TaskManager->GetThreadCount(); }

        ~TaskService() override = default;

    private:
        UInt32 m_ThreadCount{};
        Unique<TaskManager> m_TaskManager{nullptr};
    };
}

#endif // MIKOTO_TASK_MANAGER_HH
