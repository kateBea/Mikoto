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

    static auto SetupPasses(nlohmann::json &parsedJson, FrameGraph* grap) -> void {
        for (const auto &pass : parsedJson["passes"]) {
            RenderPassConfig passConfig;
            passConfig.name = pass["name"];
            passConfig.topology = pass["topology"];
            passConfig.blending = pass["blending"];
            passConfig.depthTest = pass["depthTest"];
            passConfig.cullMode = pass["cullMode"];

            for (const auto &input : pass["inputs"]) {
                passConfig.inputs.push_back(input);
            }

            for (const auto &output : pass["outputs"]) {
                passConfig.outputs.push_back(output);
            }

            for (const auto &[key, value] : pass["attachments"].items()) {
                RenderPassConfig::Attachment attachment;
                attachment.format = value["format"];
                attachment.loadOp = value["loadOp"];
                attachment.storeOp = value["storeOp"];
                passConfig.attachments[key] = attachment;
            }

            for (const auto &shader : pass["shaders"]) {
                RenderPassConfig::Shader shaderInfo;
                shaderInfo.stage = shader["stage"];
                shaderInfo.path = shader["path"];
                passConfig.shaders.push_back(shaderInfo);
            }
        }
    }

    SceneRenderer::SceneRenderer( const SceneRendererCreateInfo &createInfo )
        : m_ViewportWidth{ createInfo.ViewportWidth },
            m_ViewportHeight{ createInfo.ViewportHeight },
            m_FrameGraphPath{ createInfo.RenderGraphPath },
            m_Device{ createInfo.Device }
    {}

    auto SceneRenderer::Init() -> void {
        InitRenderPasses();

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
        m_FrameGraph->Render( m_RendererBackend );
    }

    auto SceneRenderer::OnResize( const UInt32_T width, const UInt32_T height ) -> void {
        m_ViewportWidth = width;
        m_ViewportHeight = height;

        m_FrameGraph->OnResize(m_RendererBackend, m_ViewportWidth, m_ViewportHeight);
    }

    auto SceneRenderer::Create( const SceneRendererCreateInfo &createInfo ) -> Scope_T<SceneRenderer> {
        return CreateScope<SceneRenderer>( createInfo );
    }

    auto SceneRenderer::InitRenderPasses() -> void {
        m_FrameGraph->RegisterPass( m_PassRegistry.Register<GBufferPass>() );
        m_FrameGraph->RegisterPass( m_PassRegistry.Register<ShadingPass>() );
        m_FrameGraph->RegisterPass( m_PassRegistry.Register<ShadowPass>() );
        m_FrameGraph->RegisterPass( m_PassRegistry.Register<LightGridPass>() );
        m_FrameGraph->RegisterPass( m_PassRegistry.Register<LightCullingPass>() );
        m_FrameGraph->RegisterPass( m_PassRegistry.Register<WorldTextPass>() );
        m_FrameGraph->RegisterPass( m_PassRegistry.Register<OverlayTextPass>() );
        m_FrameGraph->RegisterPass( m_PassRegistry.Register<ObjectOutlinePass>() );
    }

    auto SceneRenderer::ConstructRenderGraph() -> void {
        m_FrameBlackboard = FrameBlackboard::Create();

        constexpr FrameGraphDescription description{
            .Name{ "Scene render frame graph" },
        };

        m_FrameGraph = FrameGraph::Create( description );

        if ( m_FrameGraph != nullptr ) {
            m_FrameGraph->Init();

            ParseRenderGraph();
        }
    }

    auto SceneRenderer::ParseRenderGraph() const -> void {
        using nlohmann::json;

        const File* file{ FileService::GetInstance()->GetFile( m_FrameGraphPath ) };

        if (file == nullptr) {
            MKT_CORE_LOGGER_ERROR( "Render graph file not loaded." );
            return;
        }

        try {
            // Load JSON used to configure the passes
            json parsedJson{ json::parse(file->GetFileContents()) };

            SetupPasses( parsedJson, m_FrameGraph.get() );

        } catch (const json::parse_error& e) {
            MKT_CORE_LOGGER_ERROR("JSON parse error at byte {}: {}", e.byte, e.what());
        }
    }
}// namespace Mikoto