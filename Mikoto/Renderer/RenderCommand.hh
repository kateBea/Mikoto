/**
 * RenderCommand.cc
 * Created by kate on 6/9/23.
 * */

#ifndef MIKOTO_RENDER_COMMAND_HH
#define MIKOTO_RENDER_COMMAND_HH

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include "glm/vec4.hpp"

// Project headers
#include <Common/Common.hh>
#include <Common/Types.hh>
#include <Core/Logger.hh>
#include <Renderer/Renderer.hh>
#include <Renderer/RendererBackend.hh>

#include "IndexBuffer.hh"
#include "VertexBuffer.hh"

namespace Mikoto {
	class RenderCommand {
	public:
        static auto Init(RendererBackend * activeAPI) -> void;

        static auto EnableWireframeMode() -> void;
        static auto DisableWireframeMode() -> void;

        static auto SetClearColor(const glm::vec4& color) -> void;
        static auto SetClearColor(float red, float green, float blue, float alpha) -> void;

        static auto Flush() -> void;
        static auto AddToRenderQueue(std::shared_ptr<DrawData> &&data) -> void;

        static auto UpdateViewPort(Int32_T x, Int32_T y, Int32_T width, Int32_T height) -> void;

	private:
        inline static RendererBackend * s_ActiveRendererAPI{ nullptr };
	};
}

#endif // MIKOTO_RENDER_COMMAND_HH
