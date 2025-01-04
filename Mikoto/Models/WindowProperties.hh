//
// Created by kate on 1/4/25.
//

#ifndef WINDOWPROPERTIES_HH
#define WINDOWPROPERTIES_HH
#include <STL/Utility/Types.hh>
#include <string>

#include <Models/Enums.hh>

namespace Mikoto {
    /**
     * @brief Represents properties for a generic window.
     * */
    class WindowProperties final {
    public:
        /**
         * @brief Constructs window properties with default values.
         * @param name    The name/title of the window. Defaults to "Mikoto".
         * @param backend The graphics backend for the window. Defaults to Vulkan.
         * @param width   The width of the window. Default is 1920.
         * @param height  The height of the window. Default is 1080.
         * */
        explicit WindowProperties(std::string_view  name = "Mikoto Engine",
                                   GraphicsAPI      backend = GraphicsAPI::VULKAN_API,
                                   Int32_T          width = 1920,
                                   Int32_T          height = 1080)
            :   m_Title{ name }, m_Width{ width }, m_Height{ height }, m_Backend{ backend }
        {

        }


        /**
         * @brief Retrieves the title of the window.
         * @returns A constant reference to the string containing this window's title.
         * */
        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Title; }


        /**
         * @brief Retrieves the width of this window.
         * @returns The width of the window.
         * */
        MKT_NODISCARD auto GetWidth() const -> Int32_T { return m_Width; }


        /**
         * @brief Retrieves the height of this window.
         * @returns The height of the window.
         * */
        MKT_NODISCARD auto GetHeight() const -> Int32_T { return m_Height; }


        /**
         * @brief Retrieves the graphics backend used for this window.
         * @returns The graphics backend used for this window.
         * */
        MKT_NODISCARD auto GetBackend() const -> GraphicsAPI { return m_Backend; }


        /**
         * @brief Checks if this window is resizable.
         * @returns True if this window is resizeable, false otherwise.
         * */
        MKT_NODISCARD auto IsResizable() const -> bool { return m_Resizable; }


        /**
         * @brief Sets the width of this window.
         * @param width The width to set for this window.
         * */
        auto SetWidth(Int32_T width) -> void { m_Width = width; }


        /**
         * @brief Sets the height of this window.
         * @param height The new height for this window.
         * */
        auto SetHeight(Int32_T height) -> void { m_Height = height; }


        /**
         * @brief Sets the title of this window.
         * @param name The new title for this window.
         * */
        auto SetTitle(std::string_view name) -> void { m_Title = name; }


        /**
         * @brief Enable or disable resizing for this window.
         * @param value The value indicating whether this window should be resizable.
         * */
        auto AllowResizing(bool value) -> void { m_Resizable = value; }

    private:
        std::string m_Title{};       /**< The title of the window. */
        Int32_T     m_Width{};       /**< The width of the window. */
        Int32_T     m_Height{};      /**< The height of the window. */
        GraphicsAPI m_Backend{};     /**< The graphics backend for the window. */
        bool        m_Resizable{};   /**< Indicates if the window is resizable. */
    };
}
#endif //WINDOWPROPERTIES_HH
