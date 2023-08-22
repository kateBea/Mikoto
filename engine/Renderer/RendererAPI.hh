/**
 * RenderCommand.hh
 * Created by kate on 6/9/23.
 * */

#ifndef MIKOTO_RENDERER_API_HH
#define MIKOTO_RENDERER_API_HH

// C++ Standard Library
#include <any>
#include <memory>

// Third-Party Libraries
#include <glm/vec4.hpp>

// Project Headers
#include <Utility/Common.hh>
#include <Core/Events/Event.hh>
#include <Renderer/Buffers/IndexBuffer.hh>
#include <Renderer/Buffers/VertexBuffer.hh>
#include <Renderer/Material/Shader.hh>
#include <Renderer/RenderingUtilities.hh>


namespace Mikoto {
    /**
     * Describes a general Renderer interface for various graphic API
     * which is the graphics API being used right now. Supported API's currently
     * are Vulkan and OpenGL
     * */
	class RendererAPI {
	public:
        /**
         * Default construction
         * */
        explicit RendererAPI() = default;

        /**
         * Initializes renderer subsystems
         * */
        virtual auto Init() -> void = 0;

        /**
         * Performs cleanup for renderer subsystems
         * */
        virtual auto Shutdown() -> void = 0;

        /**
         * Enables wireframe drawing
         * */
        virtual auto EnableWireframeMode() -> void = 0;

        /**
         * Disables wireframe drawing
         * */
        virtual auto DisableWireframeMode() -> void = 0;

        /**
         * Specify the red, green, blue, and alpha values used to clear the color buffers
         * @param color contains the value for color channels
         * */
		virtual auto SetClearColor(const glm::vec4& color) -> void = 0;

        /**
         * Specify the red, green, blue, and alpha values used to clear the color buffers
         * @param red value for red channel
         * @param green value for green channel
         * @param blue value for blue channel
         * @param alpha value for alpha channel
         * */
		virtual auto SetClearColor(float red, float green, float blue, float alpha) -> void = 0;

        virtual auto Draw(const DrawData & data) -> void = 0;

		virtual auto SetViewport(UInt32_T x, UInt32_T y, UInt32_T width, UInt32_T height) -> void = 0;
        virtual auto OnEvent(Event& event) -> void = 0;

        /**
         * Default destructor.
         * */
        virtual ~RendererAPI() = default;
    public:
        RendererAPI(const RendererAPI&) = delete;
        auto operator=(const RendererAPI&) -> RendererAPI& = delete;

        RendererAPI(RendererAPI&&) = delete;
        auto operator=(RendererAPI&&) -> RendererAPI& = delete;
	};
}

#endif // MIKOTO_RENDERER_API_HH