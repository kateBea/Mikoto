//
// Created by zanet on 4/5/2025.
//

#ifndef FRAMEGRAPH_HH
#define FRAMEGRAPH_HH

#include <variant>

#include <ankerl/unordered_dense.h>

#include <Assets/Texture.hh>
#include <Common/ScopedService.hh>
#include <Library/Data/ResourcePool.hh>
#include <Library/Utility/Types.hh>

#include "Renderer/RendererBackend.hh"

namespace Mikoto {

    enum class FrameGraphResourceType {
        Invalid = -1,
        Buffer,
        Texture,
        Attachment,
        Reference,
    };

    using ExecuteCallback = std::function<auto(RendererBackend*, struct FrameGraphNode&) -> void>;

    // --- Resource Descriptions --- //

    struct BufferResourceDesc {
        Handle handle{};
    };

    struct TextureResourceDesc {
        float width{ 0.f };
        float height{ 0.f };
        float depth{ 1.f };
        float scaleWidth{ 1.f };
        float scaleHeight{ 1.f };
        TextureFormat format{};
        Handle texture{};
        std::array<float, 4> clearValues{ 0.f, 0.f, 0.f, 1.f };
        bool compute{ false };
    };

    struct FrameGraphResourceDesc {
        bool isExternal{ false };
        bool isTransient{ false };
        bool allowAliasing{ false };

        std::variant<BufferResourceDesc, TextureResourceDesc> data{};

        static auto MakeBuffer(Handle handle, bool external = false) -> FrameGraphResourceDesc {
            return {
                .isExternal = external,
                .isTransient = false,
                .allowAliasing = false,
                .data = BufferResourceDesc{ handle }
            };
        }

        static auto MakeTexture(float width, float height, TextureFormat format, Handle handle, bool external = false) -> FrameGraphResourceDesc {
            return {
                .isExternal = external,
                .isTransient = false,
                .allowAliasing = false,
                .data = TextureResourceDesc{
                    .width = width,
                    .height = height,
                    .format = format,
                    .texture = handle
                }
            };
        }

        auto IsTexture() const -> bool { return std::holds_alternative<TextureResourceDesc>(data); }
        auto IsBuffer() const -> bool { return std::holds_alternative<BufferResourceDesc>(data); }
    };

    // --- Bindings --- //

    struct FrameGraphResourceBinding {
        FrameGraphResourceType type{ FrameGraphResourceType::Invalid };
        FrameGraphResourceDesc desc{};
        std::string name{};
    };

    // --- User-Facing Pass Description --- //

    struct FrameGraphPassDesc {
        std::string name{};
        std::vector<FrameGraphResourceBinding> inputs{};
        std::vector<FrameGraphResourceBinding> outputs{};
        bool compute{ false };
        bool enabled{ true };
    };

    // --- Internal Resource --- //

    struct FrameGraphResource {
        FrameGraphResourceType type{ FrameGraphResourceType::Invalid };
        FrameGraphResourceDesc desc{};

        Handle producer{};
        Handle outputHandle{};
        int refCount{ 0 };

        std::string name{};
    };

    // --- Internal Node --- //

    struct FrameGraphNode {
        Handle renderPass{};
        Handle framebuffer{};

        ankerl::unordered_dense::map<Handle, FrameGraphResource*> inputs{};
        ankerl::unordered_dense::map<Handle, FrameGraphResource*> outputs{};

        float resolutionWidth{ 0.f };
        float resolutionHeight{ 0.f };

        bool compute{ false };
        bool enabled{ true };

        std::string name{};
        ExecuteCallback executeCallback{};
    };

    // --- Compiled Graph --- //

    struct CompiledFrameGraph {
        std::vector<FrameGraphNode*> executionOrder{};
        ankerl::unordered_dense::map<std::string, FrameGraphResource*> resourceTable{};
    };

    // --- FrameGraph API --- //

    struct FrameGraphDescription {
        std::string_view Name{};
    };

    class FrameGraph final {
    public:
        auto Init() -> void;
        auto Shutdown() -> void;

        auto Reset() -> void;

        auto EnablePass(std::string_view name) -> void;
        auto DisablePass(std::string_view name) -> void;

        auto RegisterPass(RenderPass* pass) -> void;

        auto Compile() -> void;
        auto Render(RendererBackend* renderer) -> void;
        auto OnResize(RendererBackend* renderer, uint32_t newWidth, uint32_t newHeight) -> void;

        auto GetNode(std::string_view name) -> FrameGraphNode*;
        auto GetResource(std::string_view name) -> FrameGraphResource*;

        auto DumpGraph(const std::string& filePath) const -> void;

        MKT_NODISCARD static auto Create(const FrameGraphDescription& desc) -> Scope_T<FrameGraph>;

    private:
        std::string m_DebugName{};

        ResourcePoolTyped<FrameGraphNode> m_Nodes{};
        ResourcePoolTyped<FrameGraphResource> m_Resources{};

        ankerl::unordered_dense::map<std::string, FrameGraphNode*> m_NamedNodes{};
        ankerl::unordered_dense::map<std::string, FrameGraphResource*> m_NamedResources{};
    };

    // Mediator between passes
    class FrameBlackboard {
    public:

        MKT_NODISCARD static auto Create() -> Scope_T<FrameBlackboard>;


    private:
        ankerl::unordered_dense::map<Size_T, void*> m_Data{};
    };
}// namespace Mikoto


#endif//FRAMEGRAPH_HH
