/**
 * EditorLayer.hh
 * Created by kate on 6/12/23.
 * */

#ifndef MIKOTO_EDITOR_LAYER_HH
#define MIKOTO_EDITOR_LAYER_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Layer.hh>
#include <Assets/Model.hh>
#include <Core/Serialization/SceneSerializer.hh>
#include <EditorModels/DockSpaceControlFlags.hh>
#include <EditorModels/Enums.hh>
#include <Models/Enums.hh>
#include <Panels/Panel.hh>
#include <Project/Project.hh>
#include <Scene/Camera/SceneCamera.hh>
#include <Scene/Scene/Scene.hh>
#include <Project/ProjectSerializer.hh>
#include <Renderer/Core/RendererBackend.hh>
#include <Material/Texture/TextureCubeMap.hh>

namespace Mikoto {
    struct EditorLayerCreateInfo {
        Window* TargetWindow{ nullptr };
        GraphicsAPI Backend{};
        Path_T AssetsRootDirectory{};
    };

    class EditorLayer final : public Layer {
    public:
        explicit EditorLayer() = default;
        explicit EditorLayer(const EditorLayerCreateInfo& createInfo);

        auto OnAttach() -> void override;
        auto OnDetach() -> void override;
        auto OnUpdate(double timeStep) -> void override;
        auto PushImGuiDrawItems() -> void override;

    private:
        auto SaveScene() const -> void;
        auto LoadScene() -> void;
        auto CreateScene(std::string_view name) -> void;

        auto SaveProject() -> void;
        auto OpenProject() -> void;
        auto CreateProject() -> void;

        auto CreatePanels() -> void;
        auto CreateCameras() -> void;
        auto HandleWindowScreenMode() const -> void;
        auto UpdateDockSpace() -> void;

        auto PrepareNewScene() -> void;
        auto PrepareSerialization() -> void;

        auto LoadPrefabModels() const -> void;
        auto GetPrefabModel( PrefabSceneObject type ) -> Model*;

    private:
        MKT_NODISCARD static auto GetSpritePrefabName(const std::string_view path = "") -> const std::string& { static std::string value{ path }; return value; }
        MKT_NODISCARD static auto GetCubePrefabName(const std::string_view path = "") -> const std::string& { static std::string value{ path }; return value; }
        MKT_NODISCARD static auto GetSpherePrefabName(const std::string_view path = "") -> const std::string& { static std::string value{ path }; return value; }
        MKT_NODISCARD static auto GetCylinderPrefabName(const std::string_view path = "") -> const std::string& { static std::string value{ path }; return value; }
        MKT_NODISCARD static auto GetConePrefabName(const std::string_view path = "") -> const std::string& { static std::string value{ path }; return value; }
        MKT_NODISCARD static auto GetSponzaPrefabName(const std::string_view path = "") -> const std::string& { static std::string value{ path }; return value; }

    private:
        Window* m_Window{ nullptr };

        DockControlFlags m_ControlFlags{};

        Entity* m_SelectedEntity{};

        Scope_T<Scene> m_ActiveScene{};

        TextureCubeMap* m_TextureCubeMap{};

        Scope_T<SceneCamera> m_EditorCamera{};
        Scope_T<SceneSerializer> m_SceneSerializer{};

        Scope_T<Project> m_Project{};
        Scope_T<ProjectSerializer> m_ProjectSerializer{};

        Path_T m_AssetsRootDirectory{};

        GraphicsAPI m_GraphicsAPI{};

        Scope_T<RendererBackend> m_EditorRenderer{};

        // Tells hierarchy between game objects
        //SceneGraph m_SceneGraph{};

        Registry<Panel> m_PanelRegistry{};
    };
}

#endif // MIKOTO_EDITOR_LAYER_HH
