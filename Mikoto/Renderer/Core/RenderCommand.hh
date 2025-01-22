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
#include <STL/Utility/Types.hh>
#include <Renderer/Core/Renderer.hh>
#include <Renderer/Core/RendererBackend.hh>

namespace Mikoto {
	class RenderCommand {
	public:
		virtual auto Execute() -> void = 0;
		virtual ~RenderCommand() = default;

	protected:
		explicit RenderCommand() {
			m_ActiveRendererAPI = Renderer::GetActiveGraphicsAPIPtr();
		}

        IRendererBackend* m_ActiveRendererAPI{ nullptr };
	};

	class RenderCommandSetClearColor final : public RenderCommand {
	public:
        explicit RenderCommandSetClearColor( const float red, const float green, const float blue, const float alpha)
            : m_Color{ red, green, blue, alpha }
        {}

		explicit RenderCommandSetClearColor( const glm::vec4& color )
            : m_Color{ color }
        {}

		auto Execute() -> void override {
        	m_ActiveRendererAPI->SetClearColor(m_Color);
        }

	private:
		glm::vec4 m_Color{};
	};

	class RenderCommandPushDraw final : public RenderCommand {
	public:
        explicit RenderCommandPushDraw( const std::string_view id, const GameObject& data, Material& material )
            : m_Id{ id }, m_Data{ std::addressof( data ) }, m_Material{ std::addressof( material ) } {}

		auto Execute() -> void override {
        	m_ActiveRendererAPI->QueueForDrawing(m_Id, m_Data, m_Material);
        }

        ~RenderCommandPushDraw() override = default;

    private:
        std::string m_Id{};
        const GameObject* m_Data{};
		Material* m_Material{};
    };

	class RenderCommandPopDraw final : public RenderCommand {
	public:
		explicit RenderCommandPopDraw( const std::string_view id)
		    : m_Id{ id } {}

		auto Execute() -> void override {
			m_ActiveRendererAPI->RemoveFromRenderQueue(m_Id);
		}

		~RenderCommandPopDraw() override = default;

	private:
		std::string m_Id{};
	};
}

#endif // MIKOTO_RENDER_COMMAND_HH
