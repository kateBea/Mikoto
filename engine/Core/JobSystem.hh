/**
 * JobSystem.hh
 * Created by kate on 9/6/23.
 * */

// C++ Standard Library
#include <cstdint>
#include <thread>
#include <atomic>
#include <deque>
#include <functional>
#include <condition_variable>

// Project Headers
#include <Utility/Types.hh>
#include <Utility/Common.hh>
#include <Utility/Singleton.hh>

#ifndef MIKOTO_JOB_SYSTEM_HH
#define MIKOTO_JOB_SYSTEM_HH

/**
 * Implements a simple job system.
 * This subsystem is still a WIP.
 * Will not be available for some time in the core engine
 * */
namespace Mikoto::JobSystem {
    inline UInt32_T g_ThreadCount{};                   // Count of working threads
    inline std::deque<std::function<void()>> g_JobQueue{};       // Queue of pending jobs
    inline std::condition_variable g_WakeCondition{};                  // used in conjunction with the wakeMutex below. Worker threads just sleep when there is no job, and the main thread can wake them up
    inline std::mutex g_WakeMutex{};                        // used in conjunction with the wakeCondition above
    inline UInt32_T g_CurrentLabel{};                  // tracks the state of execution of the main thread
    inline std::atomic<UInt64_T> g_FinishedLabel{};    // track the state of execution across background worker threads

    /**
     * A Dispatched job will receive this as function argument
     * */
    struct JobDispatchArgs {
        UInt32_T JobIndex;
        UInt32_T GroupIndex;
    };

    /**
     * Add job to job queue (thread safe)
     *
     * This helper exists because we are using a standard library container as job queue,
     * The standard library containers are not guaranteed to be thread safe, so function simply ensures
     * modifying the job queue is thread safe.
     * */
    auto EnqueueJob(const std::function<void()>& func) -> void {
        // use lock. STL containers are not guaranteed to be thread safe

        g_JobQueue.emplace_back(func);
    }

    /**
     * This little helper function will not let the system to be deadlocked
     * while the main thread is waiting for something
     * */
    static inline auto Poll() -> void {
        g_WakeCondition.notify_one();   // wake one worker thread
        std::this_thread::yield();      // allow this thread to be rescheduled
    }

    /**
     * Create and initialize internal resources such as threads, mutexes, etc.
     * Call once when the application initializes.
     * */
    auto Initialize() -> void;

    /**
     * Add a job to execute asynchronously. Any worker (thread) idling will execute this task.
     * @param function task to be scheduled
     * @param funcArgs function arguments
     * @tparam Args pack containing the function's arguments
     * */
    template<typename FunctionType, typename... Args>
    auto Execute(FunctionType&& function, Args&&... funcArgs) -> void {
        auto job{
            [func = std::forward<FunctionType>(function), ...args = std::forward<Args>(funcArgs)]
            () -> void {
                func(std::move(args...));
            }
        };


        // The main thread label state is updated:
        g_CurrentLabel += 1;

        // Try to push a new job until it is pushed successfully:
        EnqueueJob(job);
        Poll();

        // wake one thread
        g_WakeCondition.notify_one();
    }

    /**
     * Divide a job into multiple jobs and execute in parallel
     * @param jobCount count of jobs to generate for the task to be executed
     * @param groupSize how many jobs to execute per thread. Jobs inside a group execute serially. It might be worth to increase for small jobs
     * @param task to be executed
     * */
    auto Dispatch(UInt32_T jobCount, UInt32_T groupSize, const std::function<void(JobDispatchArgs)>& job) -> void;

    /**
     * Returns true if there's not thread executing any work, not necessarily idle.
     * @returns true if there's at least one thread doing some work, false otherwise
     * */
    MKT_NODISCARD auto IsBusy() -> bool;

    /**
     * Wait until all threads have finished doing their work
     * */
    auto WaitIdle() -> void;
}



#endif // MIKOTO_JOB_SYSTEM_HH
