//
// Created by zanet on 4/5/2025.
//

#include <FileSystem/FileService.hh>
#include <Renderer/SceneRenderer.hh>
#include <nlohmann/json.hpp>

namespace Mikoto {

    struct RenderPassConfig
    {
        std::string name;
        std::vector<std::string> inputs;
        std::vector<std::string> outputs;

        struct Attachment
        {
            std::string format;
            std::string loadOp;
            std::string storeOp;
        };

        std::unordered_map<std::string, Attachment> attachments;

        std::string topology;
        bool blending = false;
        bool depthTest = true;
        std::string cullMode;

        struct Shader
        {
            std::string stage;
            std::string path;
        };

        std::vector<Shader> shaders;
    };

    auto SceneRenderer::SetupPasses(nlohmann::json &parsedJson) -> void {
        for (const auto &pass : parsedJson["passes"]) {

        }
    }

    SceneRenderer::SceneRenderer( const SceneRendererCreateInfo &createInfo )
        : m_ViewportWidth{ createInfo.ViewportWidth },
            m_ViewportHeight{ createInfo.ViewportHeight },
            m_FrameGraphPath{ createInfo.RenderGraphPath },
            m_Device{ createInfo.Device }
    {}

    auto SceneRenderer::Init() -> void {
        ConstructRenderGraph();
    }

    auto SceneRenderer::Shutdown() -> void {
        m_FrameGraph->Shutdown();
        m_FrameGraph = nullptr;

        m_FrameBlackboard = nullptr;
    }

    auto SceneRenderer::SetState( SceneState state ) -> void {
        m_SceneState = state;
    }

    auto SceneRenderer::Render( double timeStep ) const -> void {
        m_FrameGraph->Render();
    }

    auto SceneRenderer::OnResize( const UInt32_T width, const UInt32_T height ) -> void {
        m_ViewportWidth = width;
        m_ViewportHeight = height;

        m_FrameGraph->OnResize(m_Device, m_ViewportWidth, m_ViewportHeight);
    }

    auto SceneRenderer::Create( const SceneRendererCreateInfo &createInfo ) -> Scope_T<SceneRenderer> {
        return CreateScope<SceneRenderer>( createInfo );
    }

    auto SceneRenderer::AddCoreRenderPasses() const -> void {
        m_FrameGraph->RegisterPass<GBufferPass>(m_Device);
        m_FrameGraph->RegisterPass<ShadingPass>(m_Device);
        m_FrameGraph->RegisterPass<ShadowPass>(m_Device);
        m_FrameGraph->RegisterPass<LightGridPass>(m_Device);
        m_FrameGraph->RegisterPass<LightCullingPass>(m_Device);
        m_FrameGraph->RegisterPass<WorldTextPass>(m_Device);
        m_FrameGraph->RegisterPass<OverlayTextPass>(m_Device);
        m_FrameGraph->RegisterPass<ObjectOutlinePass>(m_Device);
    }

    auto SceneRenderer::ConstructRenderGraph() -> void {
        m_FrameBlackboard = FrameBlackboard::Create();

        constexpr FrameGraphDescription description{
            .Name{ "Scene render frame graph" },
        };

        m_FrameGraph = FrameGraph::Create( description );

        if ( m_FrameGraph != nullptr ) {
            m_FrameGraph->Init();

            AddCoreRenderPasses();

            ParseRenderGraphConfig();

            m_FrameGraph->Compile();
        }
    }

    auto SceneRenderer::ParseRenderGraphConfig() -> void {
        using nlohmann::json;

        const File* file{ FileService::GetInstance()->GetFile( m_FrameGraphPath ) };

        if (!FileService::GetInstance()->LoadFile( m_FrameGraphPath )) {
            MKT_CORE_LOGGER_ERROR( "Render graph file not loaded." );
            return;
        }

        try {
            // Load JSON used to configure the passes
            json parsedJson{ json::parse(file->GetFileContents()) };

            SetupPasses( parsedJson );

        } catch (const json::parse_error& e) {
            MKT_CORE_LOGGER_ERROR("JSON parse error at byte {}: {}", e.byte, e.what());
        }
    }
}// namespace Mikoto