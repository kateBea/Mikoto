//
// Created by zanet on 4/9/2025.
//

#ifndef IPIPELINE_HH
#define IPIPELINE_HH
#include <string>

#include <Common/Common.hh>
#include <Renderer/DeviceObject.hh>
#include <Renderer/GpuUtility.hh>

#include "BufferLayout.hh"


namespace Mikoto {
    class IPipeline : public DeviceObject {
    public:
        ~IPipeline() override = default;

        MKT_NODISCARD auto GetPipelineType() const -> PipelineType {
            return m_PipelineType;
        }

        MKT_NODISCARD auto GetPassName() const -> std::string {
            return m_PassName;
        }

    protected:
        std::string m_PassName{};
        PipelineType m_PipelineType{ PipelineType::INVALID_TYPE };
    };

    struct ComputePipelineDescription {
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
    private:
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
    };

    class GraphicsPipeline : public IPipeline {
    public:
    protected:
        bool m_BackfaceCulling{ true };
        bool m_DepthTest{ true };
        bool m_DepthWrite{ true };
        bool m_StencilTest{ false };
        bool m_AlphaBlending{ false };
        bool m_Wireframe{ false };
        float m_WireframeLineWidth{ 1.0f };

        PipelineType m_PipelineType{ PipelineType::GRAPHICS_PIPELINE };
        Topology m_Topology{ Topology::TRIANGLE_LIST };
    };
}
#endif //IPIPELINE_HH
