//
// Created by zanet on 2/22/2025.
//

#ifndef RENDERVIEWPORT_HH
#define RENDERVIEWPORT_HH

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

#include <Scene/Scene/Scene.hh>
#include <Renderer/Core/RendererBackend.hh>
#include <Scene/Camera/SceneCamera.hh>

namespace Mikoto {

  struct RenderViewportCreateInfo {
    UInt32_T ViewportWidth{};
    UInt32_T ViewportHeight{};

    Scene* TargetScene{};
    RendererBackend* Renderer{};

    const SceneCamera* MainCamera{};
  };

  /**
     * Abstracts away the active ImGui render backend according
     * to the currently active graphics API.
     * */
  class RenderViewport {
  public:
    explicit RenderViewport(const RenderViewportCreateInfo& createInfo)
        : m_ViewPortWidth{ static_cast<float>( createInfo.ViewportWidth ) },
        m_ViewPortHeight{ static_cast<float>( createInfo.ViewportHeight ) },
        m_TargetScene{ createInfo.TargetScene },
        m_Renderer{ createInfo.Renderer },
        m_EditorMainCamera{ createInfo.MainCamera }
    {}

    virtual auto Init() -> void = 0;
    virtual auto OnUpdate() -> void = 0;

    auto SetEditorCamera(const SceneCamera* camera) -> void {
      m_EditorMainCamera = camera;
    }

    MKT_NODISCARD auto GetViewportWidth() const -> float { return m_ViewPortWidth; }
    MKT_NODISCARD auto GetViewportHeight() const -> float { return m_ViewPortHeight; }

    virtual ~RenderViewport() = default;

  protected:
    float m_ViewPortWidth{};
    float m_ViewPortHeight{};

    Scene* m_TargetScene{};

    RendererBackend* m_Renderer{};

    const SceneCamera* m_EditorMainCamera{};
  };
}
#endif //RENDERVIEWPORT_HH
