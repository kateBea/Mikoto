/**
 * @file RendererAPI.hh
 * @brief Provides the abstract RendererAPI interface for various graphic APIs. The engine currently supports OpenGL and Vulkan exclusively.
 * @details This file contains the definition of the RendererBackend class, an abstract interface for various graphic APIs including OpenGL and Vulkan.
 * @date 6/9/23
 * @author kate
 * */

#ifndef MIKOTO_RENDERER_API_HH
#define MIKOTO_RENDERER_API_HH

// C++ Standard Library
#include <any>
#include <memory>

// Third-Party Libraries
#include <glm/vec4.hpp>

// Project Headers
#include <Common/Common.hh>
#include <Common/RenderingUtils.hh>
#include <Material/Core/Shader.hh>
#include <Models/GameObjectData.hh>

#include "Renderer/Buffer/IndexBuffer.hh"
#include "Renderer/Buffer/VertexBuffer.hh"

namespace Mikoto {
    /**
     * @brief Describes a general Renderer interface for various graphic APIs.
     * The engine currently offers support for OpenGL and Vulkan exclusively.
     * */
    class RendererBackend {
    public:
        /**
         * @brief Default constructor. Creates and default initializes this RendererAPI.
         * */
        explicit RendererBackend() = default;


        /**
         * @brief Default base destructor
         * */
        virtual ~RendererBackend() = default;


        /**
         * @brief Initializes renderer subsystems. This function must be called
         * after creating a Renderer backend. It ensures initialization of all the
         * subsystems of the render pipeline.
         * */
        virtual auto Init() -> void = 0;


        /**
         * @brief Performs cleanup for renderer subsystems. Call as the Renderer is no
         * longer required to shut down all subsystems and perform cleanup.
         * */
        virtual auto Shutdown() -> void = 0;


        /**
         * @brief Command to flush drawing calls that need to be executed.
         * After a call to this function, all enqueued objects are dispatched to be drawn.
         * */
        virtual auto Flush() -> void = 0;


        /**
         * @brief Enables wireframe drawing.
         * */
        virtual auto EnableWireframeMode() -> void = 0;


        /**
         * @brief Disables wireframe drawing.
         * */
        virtual auto DisableWireframeMode() -> void = 0;


        /**
         * @brief Specifies the red, green, blue, and alpha values used to clear the color buffers.
         * @param color Contains the value for color channels.
         * */
        virtual auto SetClearColor(const glm::vec4& color) -> void = 0;


        /**
         * @brief Specifies the red, green, blue, and alpha values used to clear the color buffers.
         * @param red Value for the red channel.
         * @param green Value for the green channel.
         * @param blue Value for the blue channel.
         * @param alpha Value for the alpha channel.
         * */
        virtual auto SetClearColor(float red, float green, float blue, float alpha) -> void = 0;


        /**
         * @brief Sets the viewport for rendering.
         * @param x X-coordinate of the viewport.
         * @param y Y-coordinate of the viewport.
         * @param width Width of the viewport.
         * @param height Height of the viewport.
         * */
        virtual auto SetViewport(float x, float y, float width, float height) -> void = 0;


        /**
         * @brief Queues data for drawing.
         * @param data Shared pointer to the data to be drawn.
         * */
        virtual auto QueueForDrawing(const std::string &id, std::shared_ptr<GameObject> &&data, std::shared_ptr<Material> &&material) -> void = 0;


        /**
         * @brief Creates a new graphics backend object and returns a pointer to it. If
         * it fails to allocate, it returns a null pointer instead. The caller is responsible
         * for freeing the memory via delete when it's no longer necessary.
         * @param backend API backend.
         * @returns Pointer to allocated backend.
         * */
        MKT_NODISCARD static auto Create(GraphicsAPI backend) -> RendererBackend*;


        DISABLE_COPY_AND_MOVE_FOR(RendererBackend);


    private:
        /**
         * @brief Executes draw commands on the drawable objects that have been
         * added to the queue of renderable objects.
         * */
        virtual auto Draw() -> void = 0;

    };
}

#endif // MIKOTO_RENDERER_API_HH