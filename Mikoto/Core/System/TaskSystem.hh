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
#include <Library/Utility/Types.hh>
#include <Core/Engine.hh>

/**
 * Implements a simple job system.
 * This subsystem is still a WIP.
 * Will not be available/operative for some time in the engine.
 * */
namespace Mikoto {
    class TaskSystem final : public IEngineSystem {
    public:

        explicit TaskSystem() = default;

        explicit TaskSystem(const EngineConfig& options) {

        }

        ~TaskSystem() override = default;

        /**
         * A Dispatched job will receive this as function argument
         * */
        struct JobDispatchArgs {
            UInt32_T JobIndex;
            UInt32_T GroupIndex;
        };

        /**
         * Add job to job queue.
         * This helper exists because we are using a standard library container as job queue,
         * The standard library containers are not guaranteed to be thread safe, so function simply ensures
         * modifying the job queue is thread safe.
         * */
        auto EnqueueJob(std::function<void()>&& func) -> bool {
            std::scoped_lock scopedLock{ m_JobQueueLock };
            m_JobQueue.emplace_back(func);

            return true;
        }

        /**
         * Remove job to job queue.
         * */
        auto PopFrontJob(std::function<void()>& jobWrapper) -> void {
            std::scoped_lock scopedLock{ m_JobQueueLock };

            if (m_JobQueue.empty()) {
                return;
            }
            else {
                jobWrapper = m_JobQueue.front();
                m_JobQueue.pop_front();
            }
        }

        auto Init() -> void override {
            // Initialize the worker execution state to 0
            m_FinishedLabel.store(0);

            // Init state of execution of the main thread
            m_CurrentLabel = 0;

            m_ThreadCount = ComputeTotalWorkerThreads();

            // Initialize worker threads
            for (Size_T threadCount{}; threadCount < m_ThreadCount; ++threadCount) {
                m_Workers.emplace_back([this]() -> void {

                    // the current job for the thread, it's empty at the start.
                    std::function<void()> task{};

                    // This is the infinite loop that a worker thread will do
                    while (!m_Done) {
                        if (!m_JobQueue.empty()) {
                            // It found a job, execute it
                            PopFrontJob(task);

                            if (task)  {
                                task();

                                // update worker label state
                                m_FinishedLabel.fetch_add(1);

                                // Clear container after finishing the task
                                task = {};
                            }
                        }
                        else {
                            // no job, put thread to sleep
                            std::unique_lock<std::mutex> lock{ m_WakeMutex };
                            m_WakeCondition.wait(lock);
                        }
                    }

                });
            }
        }

        auto Shutdown() -> void override {
            m_Done.store(true);

            // notify all threads we are shutting down
            m_WakeCondition.notify_all();

            for (auto& worker : m_Workers) {
                worker.join();
            }
        }

        auto Update() -> void override {

        }

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
                    func(std::move(args)...);
                }
            };


            // The main thread label state is updated
            // one job is being submitted, so the Task Manager only becomes
            // idle when the worker label reached the same value
            m_CurrentLabel += 1;

            // Try to push a new job until it is pushed successfully:
            while (!EnqueueJob(job)) {
                Poll();
            }

            // wake one thread
            m_WakeCondition.notify_one();
        }

        /**
         * Divide a job into multiple jobs and execute in parallel
         * @param jobCount count of jobs to generate for the task to be executed
         * @param groupSize how many jobs to execute per thread. Jobs inside a group executed
         * sequentially. It might be worth increasing for small jobs
         * @param job the job to be executed
         * */
        auto Dispatch(UInt32_T jobCount, UInt32_T groupSize, const std::function<void(JobDispatchArgs)>& job) -> void {
            if (jobCount == 0 || groupSize == 0) {
                return;
            }

            // Calculate the number of job groups to dispatch (overestimate, or "ceil"):
            // How many worker jobs (or groups) will be put onto the jobPool
            const UInt32_T groupCount{ (jobCount + groupSize - 1) / groupSize };

            // The main thread label state is updated
            m_CurrentLabel += groupCount;

            for (UInt32_T groupIndex{}; groupIndex < groupCount; ++groupIndex) {
                // For each group, generate one real job
                auto jobGroup{
                    [jobCount, groupSize, job, groupIndex]() -> void {
                        // Calculate the current group's offset into the jobs:
                        const UInt32_T groupJobOffset{ groupIndex * groupSize };
                        const UInt32_T groupJobEnd{ std::min(groupJobOffset + groupSize, jobCount) };

                        JobDispatchArgs args{};
                        args.GroupIndex = groupIndex;

                        // Inside the group, loop through all job indices and execute job for each index:
                        for (UInt32_T i{ groupJobOffset }; i < groupJobEnd; ++i) {
                            args.JobIndex = i;
                            job(args);
                        }
                    }
                };

                // Try to push a new job until it is pushed successfully:
                while (!EnqueueJob(jobGroup)) {
                    Poll();
                }

                m_WakeCondition.notify_one();// wake one thread
            }
        }

        /**
         * Returns true if there's thread executing any work, not necessarily idle.
         * @returns true if there's at least one thread doing some work, false otherwise
         * */
        MKT_NODISCARD auto IsBusy() const -> bool {
            // Whenever the main thread label is not reached by the workers,
            // it indicates that some worker is still alive
            return m_FinishedLabel.load() < m_CurrentLabel || !m_JobQueue.empty();
        }

        /**
         * Wait until all threads have finished doing their work
         * */
        auto WaitIdle() -> void {
            while (IsBusy()) {
                Poll();
            }
        }

        MKT_NODISCARD auto GetWorkersCount() const -> UInt32_T { return m_Workers.size(); }

    private:

        /**
         * Returns total working numCores for given hardware cores
         * @param numCores count of cores (including hyper-threaded virtual cores)
         * @returns working numCores count
         * */
        static auto ComputeTotalWorkerThreads(UInt32_T numCores = std::thread::hardware_concurrency()) -> UInt32_T {
            // Calculate the actual number of worker numCores we want:
            return std::max(1u, numCores);
        }

        /**
         * This helper will not let the system to be deadlocked
         * while the main thread is waiting for something
         * */
        auto Poll() -> void {
            m_WakeCondition.notify_one();   // wake one worker thread
            std::this_thread::yield();      // allow this thread to be rescheduled
        }

    private:

        std::atomic_bool m_Done{ false };
        std::vector<std::thread> m_Workers{};

        // Count of working threads
        UInt32_T m_ThreadCount{};

        // Queue of pending jobs
        std::deque<std::function<void()>> m_JobQueue{};

        // For enqueueing and removing jobs
        std::mutex m_JobQueueLock{};

        // used in conjunction with the wakeMutex below. Worker threads
        // just sleep when there is no job, and the main thread can wake them up
        std::condition_variable m_WakeCondition{};

        // used in conjunction with the wakeCondition above
        std::mutex m_WakeMutex{};

        // tracks the state of execution of the main thread
        UInt32_T m_CurrentLabel{};

        // track the state of execution across background worker threads
        std::atomic<UInt64_T> m_FinishedLabel{};
    };
}

#endif // MIKOTO_TASK_MANAGER_HH
