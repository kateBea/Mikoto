//
// Created by zanet on 3/27/2025.
//

#ifndef THREAD_UTILS_HH
#define THREAD_UTILS_HH

#include <thread>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto::ThreadUtils {
    /**
     * @brief Get the number of concurrent threads supported by the system
     * @return Number of concurrent threads supported by the system
     */
    MKT_NODISCARD auto inline InferConcurrentThreads() -> UInt32 {
        return std::thread::hardware_concurrency();
    }
}
#endif//THREAD_UTILS_HH
