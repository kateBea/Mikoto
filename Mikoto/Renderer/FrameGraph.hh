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

    struct FrameGraphNode {
        RefAny RenderPass{};
        FramebufferHandle Framebuffer{};

        ankerl::unordered_dense::set<RefAny> Inputs{};
        ankerl::unordered_dense::set<RefAny> Outputs{};

        std::string name{};
    };

    // Mediator between passes
    class FrameBlackboard {
    public:

        MKT_NODISCARD static auto Create() -> Scope_T<FrameBlackboard> {
            return CreateScope<FrameBlackboard>();
        }

        template <typename T, typename... Args>
        auto AddResource(Args&&... args) -> T* {
            const auto& type = typeid(T);
            MKT_ASSERT(!Has<T>(), "FrameBlackboard already contains type");
            T* instance = new T(std::forward<Args>(args)...);
            m_Data[type] = instance;
            return instance;
        }

        template <typename T>
        MKT_NODISCARD auto GetResource() -> T* {
            MKT_ASSERT(Has<T>(), "Type not found in FrameBlackboard");
            return Cast<T*>(m_Data[typeid(T)]);
        }

        template <typename T>
        MKT_NODISCARD auto TryGetResource() -> T* {
            const auto it = m_Data.find(typeid(T));
            return (it != m_Data.end()) ? static_cast<T*>(it->second) : nullptr;
        }

        template <typename T>
        MKT_NODISCARD auto Has() const -> bool {
            return m_Data.contains(typeid(T));
        }

        ~FrameBlackboard() {
            for ( auto& ptr: m_Data | std::views::values ) {
                delete Cast<std::byte*>(ptr.Raw()); // treated as opaque
            }
        }

        DISABLE_COPY_AND_MOVE_FOR(FrameBlackboard);

    private:
        ankerl::unordered_dense::map<std::type_info, void*> m_Data{};
    };

    struct FrameGraphDescription {

    };

    class FrameGraph {
    public:
        virtual ~FrameGraph() = default;

        virtual auto Init() -> void = 0;
        virtual auto Shutdown() -> void = 0;

        template<typename PassType>
        auto GetPass() -> Ref<PassType> {
            auto& info{ typeid(PassType) };

            return m_Passes.contains(info) ?
                m_Passes[info].As<PassType>() : nullptr;
        }

        template<typename PassType>
        auto EnablePass(const bool value) -> void {

            if ( Ref<PassType> pass{ GetPass<PassType>() }; !pass.IsEmpty()) {
                pass->SetEnabled(value);
            }
        }

        template<typename PassType, typename... Args>
        auto RegisterPass(Args&&... args) -> void {
            const auto& info{ typeid(PassType) };
            m_Passes.try_emplace(info, new PassType(std::forward<Args>(args)...));
        }

        auto Compile() -> void;

        virtual auto OnResize(GpuDevice* device, UInt32_T newWidth, UInt32_T newHeight) -> void = 0;;

        virtual auto DumpGraph(const std::string& filePath) const -> void = 0;

        MKT_NODISCARD static auto Create(const FrameGraphDescription& desc) -> Scope_T<FrameGraph>;

    private:
        explicit FrameGraph(GraphicsAPI API);

    private:
        std::string m_DebugName{};
        FrameBlackboard* m_Blackboard{};
        ankerl::unordered_dense::map<std::type_info, RefAny> m_Passes{};
    };
}


#endif//FRAMEGRAPH_HH
