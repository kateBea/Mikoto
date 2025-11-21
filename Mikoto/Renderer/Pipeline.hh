//
// Created by zanet on 4/9/2025.
//

#ifndef IPIPELINE_HH
#define IPIPELINE_HH
#include <string>

#include <Common/Common.hh>
#include <Renderer/DeviceObject.hh>
#include <Renderer/RenderUtility.hh>
#include <Material/ShaderModule.hh>
#include <Common/ReferenceCounted.hh>

namespace Mikoto {

    class IPipeline : public DeviceObject {
    public:
        ~IPipeline() override = default;

        MKT_NODISCARD auto GetPipelineType() const -> PipelineType {
            return m_PipelineType;
        }

        auto AddStage( const ShaderModuleHandle& module ) -> void {
            if (!module.IsEmpty()) {
                m_ShaderModules.push_back(module);
            }
        }

    protected:
        explicit IPipeline(const PipelineType pipelineType)
            : m_PipelineType{ pipelineType } {}

        explicit IPipeline(const PipelineType pipelineType, const std::vector<ShaderModuleHandle>& shaderModules)
            : m_PipelineType{ pipelineType }, m_ShaderModules{ shaderModules } {}

    protected:
        const PipelineType m_PipelineType{ PipelineType::INVALID_TYPE };

        std::vector<ShaderModuleHandle> m_ShaderModules{};
    };

    using PipelineHandle = Ref<IPipeline>;

    struct ComputePipelineDescription {
        ShaderModuleHandle Stage{};
    };

    /**
    * @class ComputePipeline
    * @brief Represents a compute pipeline in the graphics API.
    *
    * This class encapsulates the properties and methods required to create and manage a compute pipeline.
    * It is designed to be used with the Vulkan graphics API.
    */
    class ComputePipeline : public IPipeline {
    public:
        explicit ComputePipeline(const ComputePipelineDescription& desc)
            : IPipeline{ PipelineType::COMPUTE_PIPELINE, { desc.Stage } } {}
    };

    // Note: for now this will always be the same layout as the Models, see Model.hh
    static inline const BufferLayout DEFAULT_VERTEX_BUFFER_LAYOUT{
        { ShaderDataType::FLOAT3_TYPE, "a_Position" },
        { ShaderDataType::FLOAT3_TYPE, "a_Normal" },
        { ShaderDataType::FLOAT3_TYPE, "a_Color" },
        { ShaderDataType::FLOAT2_TYPE, "a_TextureCoordinates" }
    };

    enum class Topology {
        POINT_LIST,
        LINE_LIST,
        LINE_STRIP,
        TRIANGLE_LIST,
        TRIANGLE_STRIP,
        TRIANGLE_FAN
    };

    enum class DepthCompareOp {
        NEVER,
        LESS,
        EQUAL,
        LESS_OR_EQUAL,
        GREATER,
        NOT_EQUAL,
        GREATER_OR_EQUAL,
        ALWAYS
    };


    struct GraphicsPipelineDescription {
        bool BackfaceCulling{ true };
        bool DepthTest{ true };
        bool DepthWrite{ true };
        bool StencilTest{ false };
        bool AlphaBlending{ false };
        bool Wireframe{ false };
        float WireframeLineWidth{ 1.0f };

        BufferLayout DefaultVertexLayout{ DEFAULT_VERTEX_BUFFER_LAYOUT };
        DepthCompareOp DepthCompareOperation{ DepthCompareOp::GREATER_OR_EQUAL };
        std::vector<ShaderModuleHandle> ShaderStages{};

        TextureHandle DepthTexture{  };
        std::vector<TextureHandle> ColorAttachments{};
    };

    /**
     * @class GraphicsPipeline
     * @brief Represents a graphics pipeline in the graphics API.
     *
     * This class encapsulates the properties and methods required to create and manage a graphics pipeline.
     * It is designed to be used with the Vulkan graphics API.
     */
    class GraphicsPipeline : public IPipeline {
    public:
        explicit GraphicsPipeline(const std::vector<ShaderModuleHandle>& modules = {})
            : IPipeline{ PipelineType::GRAPHICS_PIPELINE, modules }
        {}


        /**
         * @brief Query whether backface culling is enabled.
         * @return true if backface culling is enabled, false otherwise.
         */
        MKT_NODISCARD auto GetBackfaceCulling() const -> bool { return m_BackfaceCulling; }

        /**
         * @brief Enable or disable backface culling.
         * @param enabled true to enable, false to disable.
         */
        auto SetBackfaceCulling(bool enabled) -> void { m_BackfaceCulling = enabled; }

        /**
         * @brief Query whether depth testing is enabled.
         * @return true if depth testing is enabled, false otherwise.
         */
        MKT_NODISCARD auto GetDepthTest() const -> bool { return m_DepthTest; }

        /**
         * @brief Enable or disable depth testing.
         * @param enabled true to enable, false to disable.
         */
        auto SetDepthTest(bool enabled) -> void { m_DepthTest = enabled; }

        /**
         * @brief Query whether depth writes to the depth buffer are enabled.
         * @return true if depth writes are enabled, false otherwise.
         */
        MKT_NODISCARD auto GetDepthWrite() const -> bool { return m_DepthWrite; }

        /**
         * @brief Enable or disable writing to the depth buffer.
         * @param enabled true to enable, false to disable.
         */
        auto SetDepthWrite(bool enabled) -> void { m_DepthWrite = enabled; }

        /**
         * @brief Query whether stencil testing is enabled.
         * @return true if stencil testing is enabled, false otherwise.
         */
        MKT_NODISCARD auto GetStencilTest() const -> bool { return m_StencilTest; }

        /**
         * @brief Enable or disable stencil testing.
         * @param enabled true to enable, false to disable.
         */
        auto SetStencilTest(bool enabled) -> void { m_StencilTest = enabled; }

        /**
         * @brief Query whether alpha blending is enabled for color attachments.
         * @return true if alpha blending is enabled, false otherwise.
         */
        MKT_NODISCARD auto GetAlphaBlending() const -> bool { return m_AlphaBlending; }

        /**
         * @brief Enable or disable alpha blending for color attachments.
         * @param enabled true to enable, false to disable.
         */
        auto SetAlphaBlending(bool enabled) -> void { m_AlphaBlending = enabled; }

        /**
         * @brief Query whether polygon rendering uses wireframe mode.
         * @return true if wireframe mode is enabled, false otherwise.
         */
        MKT_NODISCARD auto GetWireframe() const -> bool { return m_Wireframe; }

        /**
         * @brief Enable or disable wireframe rendering.
         * @param enabled true to enable wireframe, false to render filled polygons.
         */
        auto SetWireframe(bool enabled) -> void { m_Wireframe = enabled; }

        /**
         * @brief Get the line width used in wireframe rendering.
         * @return The line width in pixels (or API's unit) used when wireframe is enabled.
         */
        MKT_NODISCARD auto GetWireframeLineWidth() const -> float { return m_WireframeLineWidth; }

        /**
         * @brief Set the line width for wireframe rendering.
         * @param width Line width in pixels (or API-specific units). Must be > 0.
         */
        auto SetWireframeLineWidth(float width) -> void { m_WireframeLineWidth = width; }

        /**
         * @brief Get the depth compare operation used for depth testing.
         * @return The configured DepthCompareOp.
         */
        MKT_NODISCARD auto GetDepthCompareOp() const -> DepthCompareOp { return m_DepthCompareOp; }

        /**
         * @brief Set the depth compare operation used when performing depth tests.
         * @param op DepthCompareOp value describing the comparison function.
         */
        auto SetDepthCompareOp(DepthCompareOp op) -> void { m_DepthCompareOp = op; }

        /**
         * @brief Get the primitive topology used by the pipeline (triangles, lines, etc.).
         * @return The configured Topology.
         */
        MKT_NODISCARD auto GetTopology() const -> Topology { return m_Topology; }

        /**
         * @brief Set the primitive topology for drawing.
         * @param topology The Topology to use for draw calls.
         */
        auto SetTopology(Topology topology) -> void { m_Topology = topology; }

        /**
         * @brief Get the default vertex buffer layout used by the pipeline.
         * @return A const reference to the BufferLayout used for vertex inputs.
         * @note The returned reference is non-owning; do not modify unless intended.
         */
        MKT_NODISCARD auto GetDefaultVertexLayout() const -> const BufferLayout& { return DefaultVertexLayout; }

        /**
         * @brief Set the default vertex buffer layout for the pipeline.
         * @param layout The BufferLayout describing vertex attributes and stride.
         */
        auto SetDefaultVertexLayout(const BufferLayout& layout) -> void { DefaultVertexLayout = layout; }

    protected:
        bool m_BackfaceCulling{ true };
        bool m_DepthTest{ true };
        bool m_DepthWrite{ true };
        bool m_StencilTest{ false };
        bool m_AlphaBlending{ false };
        bool m_Wireframe{ false };
        float m_WireframeLineWidth{ 1.0f };

        Topology m_Topology{ Topology::TRIANGLE_LIST };
        DepthCompareOp m_DepthCompareOp{ DepthCompareOp::ALWAYS };
        BufferLayout DefaultVertexLayout{ DEFAULT_VERTEX_BUFFER_LAYOUT };
    };
}
#endif //IPIPELINE_HH
