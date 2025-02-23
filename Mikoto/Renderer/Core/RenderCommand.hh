/**
 * RenderCommand.cc
 * Created by kate on 6/9/23.
 * */

#ifndef MIKOTO_RENDER_COMMAND_HH
#define MIKOTO_RENDER_COMMAND_HH

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <glm/vec4.hpp>

// Project headers
#include <Library/Utility/Types.hh>
#include <Renderer/Core/RendererBackend.hh>

namespace Mikoto {
	class RenderCommand {
	public:
		virtual auto Execute() -> void = 0;
		virtual ~RenderCommand() = default;

	protected:
		explicit RenderCommand(RendererBackend* api) {
			m_ActiveRendererAPI = api;
		}

        RendererBackend* m_ActiveRendererAPI{ nullptr };
	};

	class RenderCommandSetClearColor final : public RenderCommand {
	public:
        explicit RenderCommandSetClearColor( const float red, const float green, const float blue, const float alpha, RendererBackend* api )
            : RenderCommand{ api }, m_Color{ red, green, blue, alpha }
        {}

		explicit RenderCommandSetClearColor( const glm::vec4& color, RendererBackend* api )
            : RenderCommand{ api }, m_Color{ color }
        {}

		auto Execute() -> void override {
        	m_ActiveRendererAPI->SetClearColor(m_Color);
        }

	private:
		glm::vec4 m_Color{};
	};
}

#endif // MIKOTO_RENDER_COMMAND_HH
