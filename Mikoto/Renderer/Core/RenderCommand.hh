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
#include <STL/Utility/Types.hh>
#include <Core/Logger.hh>
#include <Renderer/Core/Renderer.hh>
#include <Renderer/Core/RendererBackend.hh>

#include <Renderer/Buffer/IndexBuffer.hh>
#include <Renderer/Buffer/VertexBuffer.hh>

namespace Mikoto {
	class RenderCommand {
	public:
        static auto Init(RendererBackend * activeAPI) -> void;

        static auto EnableWireframeMode() -> void;
        static auto DisableWireframeMode() -> void;

        static auto SetClearColor(const glm::vec4& color) -> void;
        static auto SetClearColor(float red, float green, float blue, float alpha) -> void;

        static auto Flush() -> void;
        static auto AddToRenderQueue(const std::string &id, std::shared_ptr<GameObject> &&data, std::shared_ptr<Material> &&material) -> void;

        static auto UpdateViewPort(Int32_T x, Int32_T y, Int32_T width, Int32_T height) -> void;

	private:
        inline static RendererBackend * s_ActiveRendererAPI{ nullptr };
	};
}

#endif // MIKOTO_RENDER_COMMAND_HH
