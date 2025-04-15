//
// Created by zanet on 4/6/2025.
//

#ifndef GRAPHICSPIPELINE_HH
#define GRAPHICSPIPELINE_HH

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/GpuDevice.hh>

#include "Renderer/Pipeline.hh"
#include <Library/Utility/Types.hh>
#include <Renderer/BufferElement.hh>
#include <Renderer/BufferLayout.hh>

namespace Mikoto {

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

        std::vector<Handle> ShaderStages{};
        GpuDevice* Device{ nullptr };
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
}// namespace Mikoto


#endif//GRAPHICSPIPELINE_HH
