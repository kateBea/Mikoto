#ifndef KATE_ENGINE_RENDER_COMMAND_HH
#define KATE_ENGINE_RENDER_COMMAND_HH

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <glm/vec4.hpp>

// Project headers
#include <Utility/Common.hh>

#include <Core/Logger.hh>

#include <Renderer/Buffers/IndexBuffer.hh>
#include <Renderer/Buffers/VertexBuffer.hh>

#include <Renderer/Renderer.hh>
#include <Renderer/RendererAPI.hh>
#include <Renderer/Material/Shader.hh>

namespace kaTe {
	class RenderCommand {
	public:
        static auto Init(RendererAPI* activeAPI) -> void;
        static auto ShutDown() -> void;

        static auto EnableWireframeMode() -> void;
        static auto DisableWireframeMode() -> void;

        static auto SetClearColor(const glm::vec4& color) -> void;
        static auto SetClearColor(float red, float green, float blue, float alpha) -> void;

        static auto Draw(const RenderingData& data) -> void;

        static auto UpdateViewPort(UInt32_T x, UInt32_T y, UInt32_T width, UInt32_T height) -> void;

	private:
        inline static RendererAPI* s_ActiveRendererAPI{ nullptr };
	};
}

#endif
