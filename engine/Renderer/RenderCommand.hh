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
#include <Utility/Common.hh>
#include <Core/Logger.hh>
#include <Renderer/Renderer.hh>
#include <Renderer/RendererAPI.hh>
#include <Renderer/Buffers/IndexBuffer.hh>
#include <Renderer/Buffers/VertexBuffer.hh>

namespace Mikoto {
	class RenderCommand {
	public:
        static auto Init(RendererAPI* activeAPI) -> void;
        static auto ShutDown() -> void;

        static auto EnableWireframeMode() -> void;
        static auto DisableWireframeMode() -> void;

        static auto SetClearColor(const glm::vec4& color) -> void;
        static auto SetClearColor(float red, float green, float blue, float alpha) -> void;

        static auto AddToRenderQueue(std::shared_ptr<DrawData>) -> void;
        static auto Flush() -> void;

        static auto UpdateViewPort(UInt32_T x, UInt32_T y, UInt32_T width, UInt32_T height) -> void;

	private:
        inline static RendererAPI* s_ActiveRendererAPI{ nullptr };
	};
}

#endif // MIKOTO_RENDER_COMMAND_HH
