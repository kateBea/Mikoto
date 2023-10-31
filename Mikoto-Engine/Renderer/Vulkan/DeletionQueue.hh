//
// Created by kate on 10/28/23.
//

#ifndef MIKOTO_DELETION_QUEUE_HH
#define MIKOTO_DELETION_QUEUE_HH

#include <deque>
#include <mutex>
#include <functional>

namespace Mikoto {
    /**
     * @class DeletionQueue
     * @brief Manages a deletion queue for deferred cleanup of resources.
     *
     * This class allows for deferring the deletion of resources by pushing deletion tasks
     * into a queue and later executing these tasks to perform cleanup. The deletion tasks
     * can be flushed, ensuring older objects get deleted first.
     * */
    class DeletionQueue {
    public:
        /**
         * @brief Pushes a deletion task into the queue.
         *
         * @param function The deletion function to be added to the queue.
         * */
        static auto Push(std::function<void()>&& function) -> void {
            std::scoped_lock lock{ s_PushMutex };
            s_DeleteTasks.emplace_back(function);
        }



        /**
         * @brief Flushes the deletion queue, executing deletion tasks.
         *
         * Reversely iterates through the deletion queue to execute the deletion tasks,
         * ensuring older objects get deleted first. After execution, the queue is cleared.
         * */
        static auto Flush() -> void {
            for (auto it{ s_DeleteTasks.rbegin() }; it != s_DeleteTasks.rend(); it++) {
                (*it)();
            }
            s_DeleteTasks.clear();
        }



    private:
        static inline std::mutex                        s_PushMutex{};     /**< Mutex for synchronized deletion task addition. */
        static inline std::deque<std::function<void()>> s_DeleteTasks{};   /**< Queue of deletion tasks. */
    };
}

#endif// MIKOTO_DELETION_QUEUE_HH
