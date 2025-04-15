
//
// Created by zanet on 3/27/2025.
//

#ifndef TASK_HH
#define TASK_HH

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <functional>
#include <limits>
#include <numeric>

#include "TaskScheduler.h"

namespace Mikoto {


    enum class TaskPriority {
        LOW,
        NORMAL,
        HIGH
    };


    class ITask {
    public:
        virtual void Execute() = 0;
        virtual ~ITask() = default;

        auto IsCompleted() const -> bool { return m_Completed; }
        auto SetIsCompleted( const bool value ) -> void { m_Completed = value; }

    protected:
        bool m_Completed{};
    };

    class IAttachedTask : public ITask, public enki::IPinnedTask {
    public:
        explicit IAttachedTask( uint32_t threadNum = 0 )
            : IPinnedTask{ threadNum }, m_ThreadNumber{ threadNum } {}

        auto GetThreadNumber() const -> uint32_t { return m_ThreadNumber; }

        auto SetThreadNumber( uint32_t number ) -> void {
            m_ThreadNumber = number;

            // enkiTS internals
            threadNum = m_ThreadNumber;
        }

        void Execute() override = 0;
        ~IAttachedTask() override = default;

    protected:
        uint32_t m_ThreadNumber{};
    };

    template<typename T>
    class Task final : public ITask {
    public:
        using TaskFunction_T = std::function<T*()>;
        using OnCompleteTaskFunction_T = std::function<void( T* )>;

        explicit Task( const TaskFunction_T& task, TaskPriority priority = TaskPriority::NORMAL )
            : m_Task{ task }, m_Priority{ priority } {
        }

        auto SetOnCompleteTask( const OnCompleteTaskFunction_T& func ) -> void {
            m_CompleteTask = func;
        }

        void Execute() override {
            m_Result = m_Task();

            m_Completed = true;

            if ( m_CompleteTask ) {
                m_CompleteTask( m_Result );
            }
        }

        auto GetObject() -> T* { return m_Result; }
        auto GetPriority() const -> TaskPriority { return m_Priority; }

    private:
        T* m_Result{};

        TaskFunction_T m_Task;
        OnCompleteTaskFunction_T m_CompleteTask;
        TaskPriority m_Priority;
    };

    template<>
    class Task<void> final : public ITask {
    public:
        using TaskFunction_T = std::function<void()>;
        using OnCompleteTaskFunction_T = std::function<void()>;

        explicit Task( const TaskFunction_T& task, const TaskPriority priority = TaskPriority::NORMAL )
            : m_Task{ task }, m_Priority{ priority } {
        }

        void Execute() override {
            m_Task();
            m_Completed = true;

            if ( m_CompleteTask ) {
                m_CompleteTask();
            }
        }

        auto SetOnCompleteTask( const OnCompleteTaskFunction_T& func ) -> void {
            m_CompleteTask = func;
        }

        auto GetPriority() const -> TaskPriority { return m_Priority; }

    private:
        TaskFunction_T m_Task;
        OnCompleteTaskFunction_T m_CompleteTask;
        TaskPriority m_Priority;
    };

    // attached executes by default on main thread (id 0)
    template<typename T>
    class AttachedTask final : public IAttachedTask {
    public:
        using TaskFunction_T = std::function<T*()>;
        using OnCompleteTaskFunction_T = std::function<void( T* )>;

        explicit AttachedTask( const TaskFunction_T& task, uint32_t threadNumber = 0, const TaskPriority priority = TaskPriority::NORMAL )
            : IAttachedTask{ threadNumber }, m_Task{ task }, m_Priority{ priority } {
        }

        auto SetOnCompleteTask( OnCompleteTaskFunction_T func ) -> void {
            m_CompleteTask = func;
        }

        void Execute() override {
            m_Result = m_Task();

            m_Completed = true;

            if ( m_CompleteTask ) {
                m_CompleteTask( m_Result );
            }
        }

        auto GetPriority() const -> TaskPriority { return m_Priority; }

    private:
        T* m_Result{};
        TaskFunction_T m_Task;
        OnCompleteTaskFunction_T m_CompleteTask;
        TaskPriority m_Priority;
    };

    template<>
    class AttachedTask<void> final : public IAttachedTask {
    public:
        using TaskFunction_T = std::function<void()>;
        using OnCompleteTaskFunction_T = std::function<void()>;

        explicit AttachedTask( const TaskFunction_T& task, uint32_t threadNumber = 0, const TaskPriority priority = TaskPriority::NORMAL )
            : IAttachedTask{ threadNumber }, m_Task{ task }, m_Priority{ priority } {
        }

        auto SetOnCompleteTask( OnCompleteTaskFunction_T func ) -> void {
            m_CompleteTask = func;
        }


        void Execute() override {
            m_Task();

            m_Completed = true;

            if ( m_CompleteTask ) {
                m_CompleteTask();
            }
        }

        auto GetPriority() const -> TaskPriority { return m_Priority; }

    private:
        TaskFunction_T m_Task;
        TaskFunction_T m_CompleteTask;
        TaskPriority m_Priority;
    };
}// namespace Mikoto


#endif//TASK_HH
