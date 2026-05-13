//
// Created by zanet on 3/27/2025.
//

#ifndef MIKOTO_TASK_MANAGER_HH
#define MIKOTO_TASK_MANAGER_HH

#include <taskflow/taskflow.hpp>
#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>

#include <Logging/Logger.hh>

#include <Threading/TaskGraph.hh>

namespace mikoto::threading {

    using namespace mikoto::core;

    class TaskManager final : public IService, public Singleton<TaskManager> {
    public:
        explicit TaskManager(tf::Executor* executor);

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        auto Submit( tf::Taskflow& flow, bool wait ) -> void;
        auto SubmitTask(std::function<void()>&& task) -> void;
        auto Execute(tf::Taskflow& flow ) -> tf::Future<void>;

        MKT_NODISCARD auto GetWorkersCount() const -> u32;

    private:
        tf::Executor* mExecutor{};
    };
}// namespace Mikoto


#endif//MIKOTO_TASK_MANAGER_HH
