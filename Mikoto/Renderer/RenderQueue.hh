
#ifndef MIKOTO_RENDER_QUEUE_HH
#define MIKOTO_RENDER_QUEUE_HH
#include <memory>
#include <queue>

#include <concurrentqueue/concurrentqueue.h>

#include "RenderCommand.hh"

namespace Mikoto {
    class RenderQueue {
    public:
        auto Init() -> void {

        }

        auto Shutdown() -> void {
            if (!m_Commands.size_approx() > 0) {
                ExecuteAll();
            }
        }

        auto Submit( RenderCommand&& cmd) -> void {
            if (!m_Commands.try_enqueue( std::move( cmd ) )) {
                MKT_CORE_LOGGER_ERROR("Failed to register render command.");
            }
        }

        auto Flush() -> void {
            ExecuteAll();
        }

    private:
        auto ExecuteAll() -> void {
            while (!m_Commands.size_approx() > 0) {

                RenderCommand command{};
                auto result{ m_Commands.try_dequeue(command) };

                if (result) {
                    command.Execute();
                }
            }
        }

        moodycamel::ConcurrentQueue<RenderCommand> m_Commands{};
    };
}

#endif