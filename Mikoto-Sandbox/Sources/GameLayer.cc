//
// Created by zanet on 10/10/2025.
//

#include "GameLayer.hh"

#include <imgui.h>

#include <ImGui/ImGuiService.hh>
#include <Logging/Logger.hh>

namespace Mikoto {

    GameLayer::GameLayer( std::string_view name )
        : ILayer{ name }
    {}

    auto GameLayer::OnUpdate( float deltaTime ) -> void {
        ImGuiIO& io{ ImGui::GetIO() };

        // Show the demo window
        ImGui::ShowDemoWindow(&m_ShowDemo);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &m_ShowDemo);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &m_ShowDemo);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            if (ImGui::ColorEdit4("clear color", reinterpret_cast<float*>( &m_ClearColor ) )) {
                ImGuiService::Get()->SetImGuiBackGroundClearColor( m_ClearColor );
            }

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (m_ShowAnotherWindow)
        {
            ImGui::Begin("Another Window", &m_ShowAnotherWindow);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                m_ShowAnotherWindow = false;
            ImGui::End();
        }
    }

    auto GameLayer::OnCreate() -> void {
        MKT_FILE_LOGGER_DEBUG( "Initializing GameLayer" );

    }

    auto GameLayer::OnDestroy() -> void {
        MKT_CORE_LOGGER_DEBUG( "Destroying Layer GameLayer..." );
    }

    auto GameLayer::OnEvent( Event &event ) -> void {

    }
}// namespace Mikoto
