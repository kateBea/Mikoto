//
// Created by zanet on 1/26/2025.
//

#ifndef GUISYSTEM_HH
#define GUISYSTEM_HH


#include <imgui.h>

#include <Core/Engine.hh>
#include <GUI/BackendImplementation.hh>
#include <deque>

namespace Mikoto {
    class GUISystem final : public IEngineSystem {
    public:
        /**
         * @brief Font names. Use these indices to access fonts retrieved from GetFonts().
         * The number at the end of the name dictates the font size
         * */
        enum GUIFonts {
            IMGUI_MANAGER_FONT_JET_BRAINS_22            = 0,
            IMGUI_MANAGER_FONT_JET_BRAINS_17            = 1,
            IMGUI_MANAGER_FONT_OPEN_SANS_17_5           = 2,
            IMGUI_MANAGER_FONT_FONT_ICON_FILE_NAME_FAR  = 3,
            IMGUI_MANAGER_FONT_FONT_ICON_FILE_NAME_MD   = 4,
            IMGUI_MANAGER_FONT_FONT_ICON_FILE_NAME_MDI  = 5,
        };

        explicit GUISystem(const EngineConfig& options)
            : m_Window{ options.TargetWindow }
        {

        }

        ~GUISystem() override = default;

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update() -> void override;

        auto EndFrame() const -> void;
        auto PrepareFrame() const -> void;

        auto GetFonts() -> std::vector<ImFont*>& { return m_Fonts; }

        template<typename... Args>
        auto AddShutdownCallback(Args&&... args) -> void {
            m_ShutdownCallbacks.emplace_back(std::forward<Args>(args)...);
        }


    private:

        auto AddIconFont(float fontSize, const std::string &path, const std::array<ImWchar, 3> &iconRanges) -> void;

        auto InitImplementation() -> void;

    private:
        Window* m_Window{ nullptr };

        inline static std::deque<std::function<void()>> m_ShutdownCallbacks{}; /**< Queue for deleting objects */

        std::vector<ImFont*> m_Fonts{};                   /**< Vector storing ImGui fonts. */
        Scope_T<BackendImplementation> m_Implementation{ nullptr }; /**< Pointer to the backend implementation. */
    };

}


#endif //GUISYSTEM_HH
