/**
 * RendererAPI.hh
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
#include <Renderer/Buffers/IndexBuffer.hh>
#include <Renderer/Buffers/VertexBuffer.hh>
#include <Renderer/Material/Shader.hh>
#include <Renderer/RenderingUtilities.hh>


namespace Mikoto {
    /**
     * Describes a general Renderer interface for various graphic APIs. The engine
     * currently offers support for OpenGL and Vulkan exclusively
     * */
	class RendererBackend {
	public:
        /**
         * Default construction. Creates and default
         * initializes this RendererAPI
         * */
        explicit RendererBackend() = default;

        /**
         * Releases all the resources held by this backend
         * */
        virtual ~RendererBackend() = default;

        /**
         * Initializes renderer subsystems. This functions has to be called
         * after creating a Renderer backend, it makes sure all the subsystems
         * of the render pipeline are initialized such as pipelines, shaders, etc.
         * */
        virtual auto Init() -> void = 0;

        /**
         * Performs cleanup for renderer subsystems. Call as the Renderer is no
         * longer required to shut down all subsystems and perform cleanup
         * */
        virtual auto Shutdown() -> void = 0;

        /**
         * Command to flush drawing calls that have to be executed. After a call to this
         * function all enqueued objects are dispatched to be drawn, the draw queue is emptied after this
         * operation.
         * */
        virtual auto Flush() -> void = 0;

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

		virtual auto SetViewport(float x, float y, float width, float height) -> void = 0;

        virtual auto QueueForDrawing(std::shared_ptr<DrawData> data) -> void = 0;

        /**
         * Creates a new graphics backend object and returns a pointer to it. If it fails to allocate
         * returns a null pointer instead. The caller is responsible for freeing the memory via delete
         * after it is no longer necessary
         * @param backend api backend
         * @returns pointer to allocated backend
         * */
        MKT_NODISCARD static auto Create(GraphicsAPI backend) -> RendererBackend*;

    public:
        DISABLE_COPY_AND_MOVE_FOR(RendererBackend);

    private:
        /**
         * Executes draw commands on the drawable objects that have been added
         * to the queue of renderable objects.
         * */
        virtual auto Draw() -> void = 0;
	};
}

#endif // MIKOTO_RENDERER_API_HH