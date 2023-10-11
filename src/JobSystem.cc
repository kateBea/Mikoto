//
// Created by kate on 9/6/23.
//

#include "Core/JobSystem.hh"

namespace Mikoto::JobSystem {
    /**
     * Returns total working numCores for given hardware cores
     * @param numCores count of cores (including hyper-threaded virtual cores)
     * @returns working numCores count
     * */
    inline static auto ComputeTotalWorkerThreads(UInt32_T numCores = std::thread::hardware_concurrency()) -> std::uint32_t {
        // Calculate the actual number of worker numCores we want:
        return std::max(1u, numCores);
    }

    auto Initialize() -> void {
        // Initialize the worker execution state to 0
        g_FinishedLabel.store(0);

        g_ThreadCount = ComputeTotalWorkerThreads();

        // Initialize worker threads
        for (UInt32_T countThreads{}; countThreads < g_ThreadCount; ++countThreads) {
            std::thread worker([]() -> void {
                // the current job for the thread, it's empty at the start.
                std::function<void()> job{};

                // This is the infinite loop that a worker thread will do
                while (true) {
                    if (!g_JobQueue.empty()) {
                        // It found a job, execute it:
                        (job = g_JobQueue.front())();

                        // Once the job is done, remove it from the list
                        g_JobQueue.pop_front();

                        // update worker label state
                        g_FinishedLabel.fetch_add(1);
                    }
                    else {
                        // no job, put thread to sleep
                        std::unique_lock<std::mutex> lock{ g_WakeMutex };
                        g_WakeCondition.wait(lock);
                    }
                }
            });

            worker.detach();
        }
    }
}