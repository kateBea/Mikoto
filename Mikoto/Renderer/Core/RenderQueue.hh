
#ifndef MIKOTO_RENDER_QUEUE_HH
#define MIKOTO_RENDER_QUEUE_HH
#include <memory>
#include <queue>

#include "RenderCommand.hh"


namespace Mikoto {
    class RenderQueue {
    public:
        static auto Init() -> void {}

        static auto Shutdown() -> void {
            if (!m_Commands.empty()) {
                ExecuteAll();
            }
        }

        static auto Submit( std::unique_ptr<RenderCommand>&& cmd) -> void {
            m_Commands.emplace( std::move( cmd ) );
        }

        static auto Flush() -> void {
            ExecuteAll();
        }

    private:
        static auto ExecuteAll() -> void {
            while (!m_Commands.empty()) {
                m_Commands.front()->Execute();
                m_Commands.pop();
            }
        }

        inline static std::queue<std::unique_ptr<RenderCommand>> m_Commands;
    };
}

#endif