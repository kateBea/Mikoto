//
// Created by kate on 10/30/25.
//

#ifndef FRAME_GRAPH_HH
#define FRAME_GRAPH_HH

#include <Assets/Texture.hh>
#include <Common/ReferenceCounted.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Buffer.hh>
#include <Renderer/Pipeline.hh>
#include <Scene/Scene.hh>

namespace Mikoto {

#if false
    // Frame resource stuff

    // Enums
    enum class Format {
        RGBA8_UNORM,
        RGBA16_FLOAT,
        D32_FLOAT
    };

    enum class TextureUsage {
        RenderTarget = 1,
        DepthStencil = 2,
        ShaderResource = 4
    };

    enum class BufferUsage {
        Vertex,
        Index,
        Uniform
    };

    enum class ResourceType {
        Texture,
        Buffer
    };

    enum class BlendState {
        Disabled,
        AlphaBlend
    };

    enum class DepthState {
        Enabled,
        Disabled
    };

    // Structs
    struct TextureDesc {
        UInt32 width;
        UInt32 height;
        Format format;
        TextureUsage usage;
    };

    struct BufferDesc {
        Size size;
        BufferUsage usage;
    };

    struct ResourceDesc {
        ResourceType type;
        std::string name;
        ResourceHandle handle;
        union {
            TextureDesc textureDesc;
            BufferDesc bufferDesc;
        };
    };

    struct LightData {
        float position[4];
        float direction[4];
        float color[4];
        float intensity;
    };

    struct MaterialData {
        float albedo[4];
        float roughness;
        float metallic;
        float ao;
    };

    class IRenderContext {
    public:
        virtual ~IRenderContext() = default;

        // Render state
        virtual auto SetRenderTarget(TextureHandle texture) -> void = 0;
        virtual auto SetRenderTarget(TextureHandle color, TextureHandle depth) -> void = 0;
        virtual auto SetViewport(int x, int y, int width, int height) -> void = 0;
        virtual auto ClearColor(float r, float g, float b, float a) -> void = 0;
        virtual auto ClearDepth(float depth) = 0;

        // Render state changes
        virtual void SetBlendState(BlendState state) = 0;
        virtual void SetDepthState(DepthState state) = 0;

        // Pipeline binding - one per pass
        virtual void BindPipeline(PipelineHandle pipeline) = 0;

        // Resource binding (for the current pipeline)
        virtual void BindTexture(const std::string& name, TextureHandle texture) = 0;
        virtual void BindBuffer(const std::string& name, BufferHandle buffer) = 0;
        virtual void BindVertexBuffer(uint32_t slot, BufferHandle buffer) = 0;
        virtual void BindIndexBuffer(BufferHandle buffer) = 0;
        virtual void BindSampler(const std::string& name, SamplerHandle sampler) = 0;

        // Push constants / dynamic data
        virtual void SetPushConstant(const std::string& name, uint32_t value) = 0;
        virtual void setPushConstant(const std::string& name, float value) = 0;

        // Drawing
        virtual auto Draw(UInt32 vertexCount) -> void = 0;
        virtual auto DrawIndexed(UInt32 indexCount) -> void = 0;
        virtual auto Dispatch(UInt32 x, UInt32 y, UInt32 z) -> void = 0;
        virtual auto DrawIndexed(UInt32 indexCount, UInt32 instanceCount, UInt32 firstIndex, Int32 vertexOffset, UInt32 firstInstance) -> void = 0;
    };

    class RenderPass {
    public:
        using ResourceHandle = Ref<IResource>;

        virtual ~RenderPass() = default;
        virtual auto Setup(FrameGraphBuilder& builder) -> void = 0;
        virtual auto Execute(IRenderContext& context) -> void = 0;
        virtual auto GetName() const -> const std::string& = 0;

    private:
        std::vector<ResourceHandle> m_Inputs{};
        std::vector<ResourceHandle> m_Outputs{};
        std::string n_Name{};
    };

    // Helper functions and structures
    class PBRPass : public RenderPass {
    private:
        // Render targets
        TextureHandle m_ColorTarget{};
        TextureHandle m_DepthTarget{};

        // Lighting resources
        TextureHandle m_ShadowMap;
        BufferHandle m_CameraMatrices;
        BufferHandle m_LightData;
        BufferHandle m_MaterialProperties{};

        // IBL resources
        TextureHandle m_EnvironmentMap{};

        // Scene data
        PipelineHandle m_PbrPipeline{};

        // We get the objects to render from the scene
        Scene* m_Scene{};

    public:
        PBRPass(Scene* scene, TextureHandle shadows)
            : m_Scene(scene), m_ShadowMap(shadows) {}

        void setup(FrameGraphBuilder& builder) override {
            // Main render targets
            colorTarget = builder.createTexture("PBRColor", {
                .width = 1920, .height = 1080,
                .format = Format::RGBA16_FLOAT,  // HDR
                .usage = static_cast<TextureUsage>(
                    static_cast<int>(TextureUsage::RenderTarget) |
                    static_cast<int>(TextureUsage::ShaderResource)
                )
            });

            depthTarget = builder.createTexture("PBRDepth", {
                .width = 1920, .height = 1080,
                .format = Format::D32_FLOAT,
                .usage = TextureUsage::DepthStencil
            });

            // Camera matrices (view, projection, view-projection)
            cameraMatrices = builder.createBuffer("CameraMatrices", {
                .size = sizeof(float) * 16 * 3,
                .usage = BufferUsage::Uniform
            });

            // Light data (position, direction, color, intensity, etc.)
            lightData = builder.createBuffer("LightData", {
                .size = sizeof(LightData) * 32,  // Support up to 32 lights
                .usage = BufferUsage::Uniform
            });

            // Material properties for each object
            materialProperties = builder.createBuffer("MaterialProps", {
                .size = sizeof(MaterialData) * opaqueObjects.size(),
                .usage = BufferUsage::Uniform
            });

            // IBL textures (could be created elsewhere)
            environmentMap = builder.readTexture("EnvironmentMap");
            irradianceMap = builder.readTexture("IrradianceMap");
            prefilterMap = builder.readTexture("PrefilterMap");
            brdfLUT = builder.readTexture("BRDFLUT");

            builder.writeTexture(colorTarget);
            builder.writeTexture(depthTarget);
        }

        void execute(RenderContext& ctx) override {
            // Set render targets
            ctx.setRenderTarget(colorTarget, depthTarget);
            ctx.setViewport(0, 0, 1920, 1080);
            ctx.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
            ctx.clearDepth(1.0f);

            // Bind PBR pipeline
            ctx.bindPipeline(pbrPipeline);

            // Bind camera and lighting data
            ctx.bindBuffer("u_cameraMatrices", cameraMatrices);
            ctx.bindBuffer("u_lightData", lightData);
            ctx.bindBuffer("u_materialProps", materialProperties);

            // Bind shadow map
            ctx.bindTexture("u_shadowMap", shadowMap);
            ctx.bindSampler("u_shadowSampler", getShadowSampler());

            // Bind IBL textures
            ctx.bindTexture("u_environmentMap", environmentMap);
            ctx.bindTexture("u_irradianceMap", irradianceMap);
            ctx.bindTexture("u_prefilterMap", prefilterMap);
            ctx.bindTexture("u_brdfLUT", brdfLUT);

            // Render all opaque objects
            for (size_t i = 0; i < opaqueObjects.size(); ++i) {
                auto& mesh = getMesh(opaqueObjects[i]);

                ctx.bindVertexBuffer(0, mesh.vertexBuffer);
                ctx.bindIndexBuffer(mesh.indexBuffer);

                // Bind object-specific textures
                ctx.bindTexture("u_albedoMap", mesh.albedoTexture);
                ctx.bindTexture("u_normalMap", mesh.normalTexture);
                ctx.bindTexture("u_roughnessMap", mesh.roughnessTexture);
                ctx.bindTexture("u_metallicMap", mesh.metallicTexture);
                ctx.bindTexture("u_aoMap", mesh.aoTexture);

                // Set material index for uniform buffer lookup
                ctx.setPushConstant("u_materialIndex", static_cast<uint32_t>(i));

                ctx.drawIndexed(mesh.indexCount);
            }
        }

        const std::string& getName() const override {
            static std::string name = "PBRPass";
            return name;
        }

        TextureHandle getColorOutput() const { return colorTarget; }
    };

    class TextPass : public RenderPass {
    private:
        TextureHandle colorTarget;  // Render on top of existing color
        TextureHandle fontAtlas;
        BufferHandle textVertices;
        BufferHandle textIndices;
        BufferHandle textTransforms;
        BufferHandle viewProjMatrix;

        PipelineHandle textPipeline;

        struct TextVertex {
            float position[2];  // Screen space position
            float texCoord[2];  // UV in font atlas
            float color[4];     // Text color
        };

        struct TextInstance {
            float transform[16];  // 4x4 transform matrix
            float color[4];       // Instance color override
        };

        std::vector<TextRenderData> textElements;

    public:
        struct TextRenderData {
            std::string text;
            float x, y;           // Screen position
            float scale;
            float color[4];
            FontHandle font;
        };

        // The color is from the final shading pass, text pass comes like at the very end
        TextPass(TextureHandle colorAttachment, const std::vector<TextRenderData>& texts)
            : colorTarget(colorBuffer), textElements(texts) {}

        void setup(FrameGraphBuilder& builder) override {
            // Read existing color buffer to render text on top
            colorTarget = builder.readTexture("PBRColor");  // From previous pass

            // Font atlas texture
            fontAtlas = builder.readTexture("FontAtlas");

            // Calculate total vertex/index count for all text
            size_t totalVertices = 0;
            size_t totalIndices = 0;
            for (const auto& text : textElements) {
                totalVertices += text.text.length() * 4;  // 4 vertices per character
                totalIndices += text.text.length() * 6;   // 2 triangles per character
            }

            // Vertex buffer for all text quads
            textVertices = builder.createBuffer("TextVertices", {
                .size = sizeof(TextVertex) * totalVertices,
                .usage = BufferUsage::Vertex
            });

            // Index buffer
            textIndices = builder.createBuffer("TextIndices", {
                .size = sizeof(uint32_t) * totalIndices,
                .usage = BufferUsage::Index
            });

            // Transform buffer for each text element
            textTransforms = builder.createBuffer("TextTransforms", {
                .size = sizeof(TextInstance) * textElements.size(),
                .usage = BufferUsage::Uniform
            });

            // View-projection matrix (usually orthographic for UI)
            viewProjMatrix = builder.createBuffer("TextViewProj", {
                .size = sizeof(float) * 16,
                .usage = BufferUsage::Uniform
            });

            builder.writeTexture(colorTarget);  // We're rendering to it
        }

        void execute(RenderContext& ctx) override {
            // Use existing color buffer as render target
            ctx.setRenderTarget(colorTarget);
            ctx.setViewport(0, 0, 1920, 1080);

            // Enable alpha blending for text
            ctx.setBlendState(BlendState::AlphaBlend);
            ctx.setDepthState(DepthState::Disabled);

            // Bind text pipeline
            ctx.bindPipeline(textPipeline);

            // Bind matrices and atlas
            ctx.bindBuffer("u_viewProjMatrix", viewProjMatrix);
            ctx.bindTexture("u_fontAtlas", fontAtlas);
            ctx.bindSampler("u_fontSampler", getFontSampler());  // Usually linear filtering

            // Bind vertex data
            ctx.bindVertexBuffer(0, textVertices);
            ctx.bindIndexBuffer(textIndices);

            // Render each text element
            uint32_t indexOffset = 0;
            for (size_t i = 0; i < textElements.size(); ++i) {
                const auto& textData = textElements[i];

                // Set instance data (transform, color)
                ctx.setPushConstant("u_instanceIndex", static_cast<uint32_t>(i));

                // Draw this text string
                uint32_t indexCount = static_cast<uint32_t>(textData.text.length() * 6);
                ctx.drawIndexed(indexCount, 1, indexOffset, 0, 0);

                indexOffset += indexCount;
            }

            // Restore render state
            ctx.setBlendState(BlendState::Disabled);
            ctx.setDepthState(DepthState::Enabled);
        }

        const std::string& getName() const override {
            static std::string name = "TextPass";
            return name;
        }

        // Helper to generate vertex data for text (called during resource setup)
        void generateTextGeometry(const std::string& text, float x, float y,
                                 float scale, const float color[4],
                                 const FontData& font,
                                 std::vector<TextVertex>& vertices,
                                 std::vector<uint32_t>& indices) {
            float currentX = x;
            uint32_t baseVertex = static_cast<uint32_t>(vertices.size());

            for (char c : text) {
                const auto& glyph = font.getGlyph(c);

                // Create quad for this character
                float x0 = currentX + glyph.bearingX * scale;
                float y0 = y + glyph.bearingY * scale;
                float x1 = x0 + glyph.width * scale;
                float y1 = y0 - glyph.height * scale;

                // Add 4 vertices
                vertices.push_back({{x0, y0}, {glyph.u0, glyph.v0}, {color[0], color[1], color[2], color[3]}});
                vertices.push_back({{x1, y0}, {glyph.u1, glyph.v0}, {color[0], color[1], color[2], color[3]}});
                vertices.push_back({{x1, y1}, {glyph.u1, glyph.v1}, {color[0], color[1], color[2], color[3]}});
                vertices.push_back({{x0, y1}, {glyph.u0, glyph.v1}, {color[0], color[1], color[2], color[3]}});

                // Add 6 indices (2 triangles)
                indices.insert(indices.end(), {
                    baseVertex + 0, baseVertex + 1, baseVertex + 2,
                    baseVertex + 0, baseVertex + 2, baseVertex + 3
                });

                baseVertex += 4;
                currentX += glyph.advance * scale;
            }
        }
    };

    class ShadowPass : public RenderPass {
    private:
        TextureHandle shadowMap;
        BufferHandle lightViewProjMatrix;
        BufferHandle shadowCasterTransforms;
        PipelineHandle shadowPipeline;

        // Input data
        // We get the shadow casters from the scene
        Scene* m_Scene{};
        LightHandle directionalLight;

    public:
        ShadowPass(LightHandle light, const std::vector<MeshHandle>& casters)
            : directionalLight(light), shadowCasters(casters) {}

        void setup(FrameGraphBuilder& builder) override {
            // Create shadow map
            shadowMap = builder.createTexture("ShadowMap", {
                .width = 2048, .height = 2048,
                .format = Format::D32_FLOAT,
                .usage = static_cast<TextureUsage>(
                    static_cast<int>(TextureUsage::DepthStencil) |
                    static_cast<int>(TextureUsage::ShaderResource)
                )
            });

            // Light's view-projection matrix
            lightViewProjMatrix = builder.createBuffer("LightViewProj", {
                .size = sizeof(float) * 16,
                .usage = BufferUsage::Uniform
            });

            // Instance transforms for shadow casters
            shadowCasterTransforms = builder.createBuffer("ShadowCasterTransforms", {
                .size = sizeof(float) * 16 * shadowCasters.size(),
                .usage = BufferUsage::Uniform
            });

            builder.writeTexture(shadowMap);
        }

        void execute(RenderContext& ctx) override {
            // Set up shadow map rendering
            ctx.setRenderTarget(shadowMap);
            ctx.setViewport(0, 0, 2048, 2048);
            ctx.clearDepth(1.0f);

            // Bind shadow pipeline (depth-only)
            ctx.bindPipeline(shadowPipeline);
            ctx.bindBuffer("u_lightViewProj", lightViewProjMatrix);
            ctx.bindBuffer("u_transforms", shadowCasterTransforms);

            // Render all shadow casting objects
            for (size_t i = 0; i < shadowCasters.size(); ++i) {
                auto& mesh = getMesh(shadowCasters[i]);

                ctx.bindVertexBuffer(0, mesh.vertexBuffer);
                ctx.bindIndexBuffer(mesh.indexBuffer);

                // Set instance index for transform lookup
                ctx.setPushConstant("u_instanceIndex", static_cast<uint32_t>(i));
                ctx.drawIndexed(mesh.indexCount);
            }
        }

        const std::string& getName() const override {
            static std::string name = "ShadowPass";
            return name;
        }

        TextureHandle getShadowMap() const { return shadowMap; }


    };

    class FrameGraphBuilder {
    private:
        FrameGraph& frameGraph;

    public:
        FrameGraphBuilder(FrameGraph& fg) : frameGraph(fg) {}

        TextureHandle createTexture(const std::string& name, const TextureDesc& desc);
        BufferHandle createBuffer(const std::string& name, const BufferDesc& desc);
        TextureHandle readTexture(const std::string& name);
        void writeTexture(TextureHandle handle);
    };

    class FrameGraph {
    private:
        std::vector<std::unique_ptr<RenderPass>> passes;
        std::unordered_map<std::string, ResourceHandle> namedResources;
        std::vector<ResourceDesc> resourceDescs;
        bool compiled = false;

    public:
        // Add platform-agnostic passes
        void addPass(std::unique_ptr<RenderPass> pass) {
            passes.push_back(std::move(pass));
        }

        // Build phase - let passes declare their resources
        void build() {
            FrameGraphBuilder builder(*this);

            for (auto& pass : passes) {
                pass->setup(builder);  // Each pass declares what it needs
            }

            // Analyze dependencies, optimize resource usage, etc.
            analyzeDependencies();
            optimizeResources();
        }

        // Compile for specific backend
        void compile(RenderBackend& backend);

        // Execute all passes
        void execute(RenderBackend& backend);

        // Internal methods
        void addResource(const std::string& name, const ResourceDesc& desc) {
            ResourceHandle handle = static_cast<ResourceHandle>(resourceDescs.size());
            resourceDescs.push_back(desc);
            namedResources[name] = handle;
        }

        ResourceHandle getResource(const std::string& name) {
            auto it = namedResources.find(name);
            if (it != namedResources.end()) {
                return it->second;
            }
            // Return invalid handle or throw exception
            return static_cast<ResourceHandle>(-1);
        }

        const std::vector<std::unique_ptr<RenderPass>>& getPasses() const { return passes; }
        const std::vector<ResourceDesc>& getResourceDescs() const { return resourceDescs; }

    private:
        void analyzeDependencies() {
            // TODO: Implement dependency analysis
        }

        void optimizeResources() {
            // TODO: Implement resource optimization
        }
    };

    // Implementation of FrameGraphBuilder methods
    inline TextureHandle FrameGraphBuilder::createTexture(const std::string& name, const TextureDesc& desc) {
        ResourceDesc resourceDesc;
        resourceDesc.type = ResourceType::Texture;
        resourceDesc.name = name;
        resourceDesc.textureDesc = desc;

        frameGraph.addResource(name, resourceDesc);
        return frameGraph.getResource(name);
    }

    inline BufferHandle FrameGraphBuilder::createBuffer(const std::string& name, const BufferDesc& desc) {
        ResourceDesc resourceDesc;
        resourceDesc.type = ResourceType::Buffer;
        resourceDesc.name = name;
        resourceDesc.bufferDesc = desc;

        frameGraph.addResource(name, resourceDesc);
        return frameGraph.getResource(name);
    }

    inline TextureHandle FrameGraphBuilder::readTexture(const std::string& name) {
        return frameGraph.getResource(name);
    }

    inline void FrameGraphBuilder::writeTexture(TextureHandle handle) {
        // Mark this resource as written to (for dependency analysis)
        // TODO: Implement dependency tracking
    }

    class FrameGraphCompiler {
    public:
        void compile(const FrameGraph& graph, RenderBackend& backend);

    private:
        void compileResources(RenderPass* pass, RenderBackend& backend);
    };

#endif
}



#endif
