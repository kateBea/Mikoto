//
// Created by kate on 10/28/23.
//

#ifndef MIKOTO_DELETION_QUEUE_HH
#define MIKOTO_DELETION_QUEUE_HH

#include <deque>
#include <mutex>
#include <functional>

#include <STL/Utility/Types.hh>
#include <Renderer/Vulkan/VulkanObject.hh>

namespace Mikoto {
    /**
     * @class DeletionQueue
     * @brief Manages a deletion queue for deferred cleanup of resources.
     * This class allows for deferring the deletion of resources by pushing deletion tasks
     * into a queue and later executing these tasks to perform cleanup. The deletion tasks
     * can be flushed, ensuring older objects get deleted first.
     * */
    class DeletionQueue final {
    public:
        /**
         * @brief Pushes a deletion task into the queue.
         * @param function The deletion function to be added to the queue.
         * */
        static auto Push(std::function<void()>&& function) -> void {
            std::scoped_lock lock{ s_PushMutex };
            s_DeleteTasks.emplace_back(function);
        }

        /**
         * @brief Registers a Vulkan object for deletion.
         * @param obj The Vulkan object to be registered for deletion.
         * */
        static auto Register(Ref_T<VulkanObject> obj) -> void {
            s_Objects.emplace_back(obj);
        }

        /**
         * @brief Releases Vulkan objects that are marked for deletion.
         * Iterates through the list of Vulkan objects and releases the objects that are marked for deletion.
         * */
        static auto Release() -> void {
            for ( auto objectIt{ s_Objects.begin() }; objectIt != s_Objects.end(); ++objectIt ) {

                if ( const auto& obj{ *objectIt }; obj->ShouldRelease()) {
                    obj->Release();
                    obj->Invalidate();
                }
            }
        }

        /**
         * @brief Flushes the deletion queue, executing deletion tasks.
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
        /** Mutex for synchronized deletion task addition. */
        static inline std::mutex s_PushMutex{};

        /** Queue of deletion tasks. */
        static inline std::deque<std::function<void()>> s_DeleteTasks{};
        static inline std::deque<Ref_T<VulkanObject>> s_Objects{};
    };
}

#endif// MIKOTO_DELETION_QUEUE_HH
