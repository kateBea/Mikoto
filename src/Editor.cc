/**
 * Editor.cc
 * Created by kate on 6/23/23.
 * */

// Third-Party Libraries
#include <imgui.h>

// Project Headers
#include <Editor/Editor.hh>

namespace Mikoto::Editor {
    auto ThemeDarkModeAlt() -> void {
        // Setup Dear ImGui style
        ImGuiStyle& style = ImGui::GetStyle();

        style.Colors[ImGuiCol_WindowBg] = ImVec4{ 0.1f, 0.105f, 0.11f, 1.0f };

        // Headers
        style.Colors[ImGuiCol_Header] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
        style.Colors[ImGuiCol_HeaderActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

        // Buttons
        style.Colors[ImGuiCol_Button] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
        style.Colors[ImGuiCol_ButtonActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

        // Frame BG
        style.Colors[ImGuiCol_FrameBg] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

        // Tabs
        style.Colors[ImGuiCol_Tab] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
        style.Colors[ImGuiCol_TabHovered] = ImVec4{ 0.38f, 0.3805f, 0.381f, 1.0f };
        style.Colors[ImGuiCol_TabActive] = ImVec4{ 0.28f, 0.2805f, 0.281f, 1.0f };
        style.Colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
        style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };

        // Title
        style.Colors[ImGuiCol_TitleBg] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

        // borders
        style.WindowBorderSize = 0.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 0.0f;

        // Rounding values
        style.FrameRounding = .5f;
        style.GrabRounding = .5f;
        style.ChildRounding = .5f;
        style.WindowRounding = .5f;
        style.PopupRounding = .5f;
        style.ScrollbarRounding = .5f;
        style.TabRounding = .5f;
    }

    auto ThemeDarkModeDefault() -> void {
        // Setup Dear ImGui style
        ImGuiStyle &style = ImGui::GetStyle();

        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);

        style.Colors[ImGuiCol_Tab] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);

        style.Colors[ImGuiCol_Button] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.26f, 0.26f, 1.0f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);

        style.Colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.26f, 0.26f, 1.0f);
        style.Colors[ImGuiCol_TabActive] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
        style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);
        style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);

        style.Colors[ImGuiCol_Header] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.26f, 0.26f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);

        style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);

        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);

        style.Colors[ImGuiCol_Border] = ImVec4(0.26f, 0.26f, 0.26f, 1.0f);
        style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);

        style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.10f, 0.10f, 0.10f, 1.0f);
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);


        // borders
        style.WindowBorderSize = 0.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 0.0f;

        // Rounding values
        style.FrameRounding = .5f;
        style.GrabRounding = .5f;
        style.ChildRounding = .5f;
        style.WindowRounding = .5f;
        style.PopupRounding = .5f;
        style.ScrollbarRounding = .5f;
        style.TabRounding = .5f;
    }


    auto ShowDockingDisabledMessage() -> void {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("ERROR: Docking is not enabled! See Demo > Configuration.");
        ImGui::Text("Set io.ConfigFlags |= ImGuiConfigFlags_DockingEnable in your code, or ");
        ImGui::SameLine(0.0f, 0.0f);
        if (ImGui::SmallButton("click here"))
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    }

    auto HelpMarker(std::string_view description) -> void {
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && ImGui::BeginTooltip()) {
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(description.data());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    auto OnDockSpaceUpdate() -> void {
        // If you strip some features of, this demo is pretty much equivalent to calling DockSpaceOverViewport()!
        // In most cases you should be able to just call DockSpaceOverViewport() and ignore all the code below!
        // In this specific demo, we are not using DockSpaceOverViewport() because:
        // - we allow the host window to be floating/moveable instead of filling the viewport (when opt_fullscreen == false)
        // - we allow the host window to have padding (when optPadding == true)
        // - we have a local menu bar in the host window (vs. you could use BeginMainMenuBar() + DockSpaceOverViewport() in your code!)
        // TL;DR; this demo is more complicated than what you would normally use.
        // If we removed all the options we are showcasing, this demo would become:
        //     void ShowExampleAppDockSpace()
        //     {
        //         ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
        //     }

        static bool optPadding{ false };
        static ImGuiDockNodeFlags dockSpaceConfigFlags = ImGuiDockNodeFlags_None;

        // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
        // because it would be confusing to have two docking targets within each others.
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

        // Dockspace always takes the full screen
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
        // and handle the pass-thru hole, so we ask Begin() to not render a background.
        if (dockSpaceConfigFlags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;

        // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
        // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
        // all active windows docked into it will lose their parent and become undocked.
        // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
        // any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
        if (!optPadding)
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace Demo", &s_ControlFlags.ApplicationCloseFlag, window_flags);
        if (!optPadding)
            ImGui::PopStyleVar();

        // DockSpace is always fullscreen
        ImGui::PopStyleVar(2);

        // Submit the DockSpace
        ImGuiIO& io = ImGui::GetIO();
        ImGuiStyle& style = ImGui::GetStyle();

        float minimumPanelsWidth{ style.WindowMinSize.x }; // minimum imgui windows width (temporary)
        style.WindowMinSize.x = 450;
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            ImGuiID dockSpaceId = ImGui::GetID("kaTeDockEditor");
            ImGui::DockSpace(dockSpaceId, ImVec2(0.0f, 0.0f), dockSpaceConfigFlags);
        }
        else
            ShowDockingDisabledMessage();

        style.WindowMinSize.x = minimumPanelsWidth;

        if (ImGui::BeginMenuBar()) {
            // File menubar. Need to extract to separated function
            if (ImGui::BeginMenu("File")) {
                // Disabling fullscreen would allow the window to be moved to the front of other windows,
                // which we can't undo at the moment without finer window depth/z control.
                ImGui::MenuItem("Open");
                ImGui::MenuItem("Save");
                ImGui::Separator();

                ImGui::Separator();

                if (ImGui::MenuItem("Close", nullptr, false))
                    s_ControlFlags.ApplicationCloseFlag = true;

                ImGui::EndMenu();
            }

            HelpMarker(
                    "When docking is enabled, you can ALWAYS dock MOST window into another! Try it now!" "\n"
                    "- Drag from window title bar or their tab to dock/undock." "\n"
                    "- Drag from window menu button (upper-left button) to undock an entire node (all windows)." "\n"
                    "- Hold SHIFT to disable docking (if io.ConfigDockingWithShift == false, default)" "\n"
                    "- Hold SHIFT to enable docking (if io.ConfigDockingWithShift == true)" "\n"
                    "This demo app has nothing to do with enabling docking!" "\n\n"
                    "This demo app only demonstrate the use of ImGui::DockSpace() which allows you to manually create a docking node _within_ another window." "\n\n"
                    "Read comments in ShowExampleAppDockSpace() for more details.");

            // Window. Need to extract to separated function
            if (ImGui::BeginMenu("Window")) {
                if (ImGui::BeginMenu("Panels")) {
                    if (ImGui::MenuItem("Hierarchy", nullptr, s_ControlFlags.HierarchyPanelVisible))   s_ControlFlags.HierarchyPanelVisible = !s_ControlFlags.HierarchyPanelVisible;
                    if (ImGui::MenuItem("Inspector", nullptr, s_ControlFlags.InspectorPanelVisible))   s_ControlFlags.InspectorPanelVisible = !s_ControlFlags.InspectorPanelVisible;
                    if (ImGui::MenuItem("Scene", nullptr, s_ControlFlags.ScenePanelVisible))       s_ControlFlags.ScenePanelVisible = !s_ControlFlags.ScenePanelVisible;
                    if (ImGui::MenuItem("Settings", nullptr, s_ControlFlags.SettingPanelVisible))    s_ControlFlags.SettingPanelVisible = !s_ControlFlags.SettingPanelVisible;
                    if (ImGui::MenuItem("Statistics", nullptr, s_ControlFlags.StatsPanelVisible))    s_ControlFlags.StatsPanelVisible = !s_ControlFlags.StatsPanelVisible;
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Theme")) {
                    if (ImGui::MenuItem("Classic")) {
                        ImGui::StyleColorsClassic();
                    }
                    if (ImGui::MenuItem("Dark Default")) {
                        ImGui::StyleColorsDark();
                        ThemeDarkModeDefault();
                    }
                    if (ImGui::MenuItem("Dark Alternative")) {
                        ImGui::StyleColorsDark();
                        ThemeDarkModeAlt();
                    }
                    if (ImGui::MenuItem("Focused")) {
                        ImGui::StyleColorsDark();
                    }
                    if (ImGui::MenuItem("Blindness")) {
                        ImGui::StyleColorsLight();
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }

            HelpMarker("This help is temporary. This menu helps to change window stuff like the theme");

            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About"))
                    ImGui::OpenPopup("AboutPopUp11111");

                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        ImGui::End();
    }

    auto DrawAboutModalPopup() -> void {
        // Always center this window when appearing
        ImVec2 center{ ImGui::GetMainViewport()->GetCenter() };
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("AboutPopUp11111", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("GPU");
            ImGui::Text("Vendor");
            ImGui::Text("Driver Version");

            if (ImGui::Button("Accept", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}