/**
 * ImGuiLayer.hh
 * Created by kate on 5/28/23.
 * */

#ifndef MIKOTO_IMGUI_LAYER_HH
#define MIKOTO_IMGUI_LAYER_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <GUI/ImGuiUtils.hh>
#include <GUI/ImGuiVulkanBackend.hh>
#include <Platform/Window/Window.hh>
#include <deque>

namespace Mikoto {
    /**
     * @brief ImGuiLayer interface for ImGui GUI elements.
     * Uses OpenGL/Vulkan for rendering and GLFW for event handling.
     * */
    class ImGuiManager {
    public:
        /**
         * @brief Font names. Use these indices to access fonts retrieved from GetFonts().
         * The number at the end of the name dictates the font size
         * */
        enum ImGuiManagerFont {
            IMGUI_MANAGER_FONT_JET_BRAINS_22            = 0,
            IMGUI_MANAGER_FONT_JET_BRAINS_17            = 1,
            IMGUI_MANAGER_FONT_OPEN_SANS_17_5           = 2,
            IMGUI_MANAGER_FONT_FONT_ICON_FILE_NAME_FAR  = 3,
            IMGUI_MANAGER_FONT_FONT_ICON_FILE_NAME_MD   = 4,
            IMGUI_MANAGER_FONT_FONT_ICON_FILE_NAME_MDI  = 5,
        };

        /**
         * @brief Initializes the ImGuiManager with the provided window.
         * @param window Shared pointer to the window to associate with ImGui.
         * */
        static auto Init(const Window* window) -> void;

        /**
         * @brief Starts a new ImGui frame. Must be called before submitting any ImGui item.
         * */
        static auto BeginFrame() -> void;

        /**
         * @brief Ends an ImGui frame. Call to submit ImGui items to be rendered.
         * */
        static auto EndFrame() -> void;

        /**
         * @brief Retrieves the vector of ImGui fonts.
         * @return Vector containing ImGui fonts.
         * */
        static auto GetFonts() -> std::vector<ImFont*>& { return s_Fonts; }

      template<typename... Args>
      static auto AddShutdownCallback(Args&&... args) -> void {
          s_ShutdownCallbacks.emplace_back(std::forward<Args>(args)...);
        }

        /**
         * @brief Destroys the ImGui context.
         * */
        static auto Shutdown() -> void;

    private:
        /**
         * @brief Adds an icon font to the ImGui manager.
         * @param fontSize Size of the font to add.
         * @param path Path to the font file.
         * @param iconRanges Ranges of the icon font.
         * */
        static auto AddIconFont(float fontSize, const std::string &path, const std::array<ImWchar, 3> &iconRanges) -> void;

        /**
         * @brief Initializes the underlying implementation for ImGui.
         * Must be called after OnAttach. The implementation supports Vulkan and OpenGL, both
         * of which work with GLFW for window management and event handling. Therefore, the window's native handle must be from GLFW.
         * The behavior is undefined if the window's native handle is not of GLFW.
         * @param window Shared pointer to the window handle.
         * */
        static auto InitImplementation(const Window* window) -> void;

    private:
      inline static std::deque < std::function<void()>> s_ShutdownCallbacks{}; /**< Queue for deleting objects */
      inline static std::vector<ImFont*> s_Fonts{};                   /**< Vector storing ImGui fonts. */
        inline static Scope_T<BackendImplementation> m_Implementation{ nullptr }; /**< Pointer to the backend implementation. */
    };
}

#endif // MIKOTO_IMGUI_LAYER_HH
