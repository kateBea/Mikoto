/**
 * MainWindow.cc
 * Created by kate on 5/26/23.
 * */

// C++ Standard Library
#include <any>
#include <memory>

// Third-Party Libraries
#include <volk.h>
#include <GLFW/glfw3.h>

// Projects headers
#include <Utility/Types.hh>
#include <Utility/Common.hh>
#include <Core/Assert.hh>
#include <Core/Logger.hh>
#include <Core/CoreEvents.hh>
#include <Core/EventManager.hh>
#include <Platform/MainWindow.hh>
#include <Platform/InputManager.hh>
#include <Renderer/RenderContext.hh>
#include <Renderer/RenderingUtilities.hh>

namespace Mikoto {
    MainWindow::MainWindow(WindowProperties&& properties)
        :   Window{ std::move(properties) }, m_Window{ nullptr }
    {

    }

    auto MainWindow::Present() -> void {

        RenderContext::Present();
    }

    auto MainWindow::Init() -> void {
        MKT_CORE_LOGGER_INFO("Main Window initialization");

        // Initialize GLFW Library
        InitGLFW();

        // Major and minor values for render backend
        UInt32_T major{};
        UInt32_T minor{};

        switch(m_Properties.GetBackend()) {
            case GraphicsAPI::OPENGL_API:
                major = MKT_OPENGL_VERSION_MAJOR;
                minor = MKT_OPENGL_VERSION_MINOR;
                m_Properties.SetTitle(fmt::format("Mikoto (OpenGL Version {}.{}.0)", major, minor));
                break;
            case GraphicsAPI::VULKAN_API:
                major = MKT_VULKAN_VERSION_MAJOR;
                minor = MKT_VULKAN_VERSION_MINOR;
                m_Properties.SetTitle(fmt::format("Mikoto (Vulkan Version {}.{})", major, minor));

                // Because GLFW was originally designed to create an OpenGL context,
                // we need to tell it to not create an OpenGL context with a subsequent call to glfwCreateWindow
                glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
                break;
        }

        // Allow resizing?
        if (IsResizable()) {
            glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        }
        else {
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        }

        GLFWWindowCreateSpec spec{};
        spec.Width = GetWidth();
        spec.Height = GetHeight();
        spec.Title = GetTitle();

        m_Window = CreateGLFWWindow(spec);

        m_WindowCreateSuccess = m_Window != nullptr;
        MKT_ASSERT(m_WindowCreateSuccess, "Failed to create the Window GLFW");
        MKT_CORE_LOGGER_INFO("Created GLFW Window with name '{}'", m_Properties.GetName());
        MKT_CORE_LOGGER_INFO("Created GLFW Window with dim [{}, {}]", m_Properties.GetWidth(), m_Properties.GetHeight());

        SpawnOnCenter();
        InstallCallbacks();
    }

    auto MainWindow::Shutdown() -> void {
        MKT_CORE_LOGGER_INFO("Shutting down GLFW Window with name '{}'", m_Properties.GetName());
        MKT_CORE_LOGGER_INFO("GLFW Window dimensions are [{}, {}]", m_Properties.GetWidth(), m_Properties.GetHeight());

        DestroyGLFWWindow(m_Window);
    }

    auto MainWindow::InstallCallbacks() -> void {
        glfwSetWindowUserPointer(m_Window, this);

        glfwSetWindowSizeCallback(m_Window,
            [](GLFWwindow* window, Int32_T width, Int32_T height) -> void {
                MainWindow* data{ static_cast<MainWindow *>(glfwGetWindowUserPointer(window)) };
                data->m_Properties.SetWidth(width);
                data->m_Properties.SetHeight(height);

                EventManager::Trigger<WindowResizedEvent>(width, height);
            }
        );

        glfwSetWindowCloseCallback(m_Window,
            [](GLFWwindow* window) {
                EventManager::Trigger<WindowCloseEvent>();
            }
        );

        glfwSetKeyCallback(m_Window,
            [](GLFWwindow *window, std::int32_t key, [[maybe_unused]] Int32_T  scancode, Int32_T action, Int32_T mods) -> void {
                const MainWindow* data{ static_cast<MainWindow *>(glfwGetWindowUserPointer(window)) };

                switch (action) {
                    case GLFW_PRESS: {
                        EventManager::Trigger<KeyPressedEvent>(key, false, mods);
                        break;
                    }
                    case GLFW_RELEASE: {
                        EventManager::Trigger<KeyReleasedEvent>(key);
                        break;
                    }
                    case GLFW_REPEAT: {
                        EventManager::Trigger<KeyPressedEvent>(key, true, mods);
                        break;
                    }
                    default: {
                        MKT_CORE_LOGGER_WARN("Unknown Key action for key callback");
                        break;
                    }
                }
            }
        );

        glfwSetMouseButtonCallback(m_Window,
            [](GLFWwindow* window, Int32_T button, Int32_T action, Int32_T mods) -> void {
                switch (action) {
                    case GLFW_PRESS: {
                        EventManager::Trigger<MouseButtonPressedEvent>(button, mods);
                        break;
                    }
                    case GLFW_RELEASE: {
                        EventManager::Trigger<MouseButtonReleasedEvent>(button);
                        break;
                    }
                    default:
                        MKT_CORE_LOGGER_WARN("Unknown GLFW_ value for glfwSetMouseButtonCallback");
                        break;
                }
            }
        );

        glfwSetScrollCallback(m_Window,
            [](GLFWwindow* window, double xOffset, double yOffset) -> void {
                EventManager::Trigger<MouseScrollEvent>(xOffset, yOffset);
            }
        );

        glfwSetCursorPosCallback(m_Window,
            [](GLFWwindow* window, double x, double y) -> void {
                EventManager::Trigger<MouseMovedEvent>(x, y);
            }
        );

        glfwSetCharCallback(m_Window,
            [](GLFWwindow* window, unsigned int codePoint) -> void {
                EventManager::Trigger<KeyCharEvent>(codePoint);
            }
        );

        // This function will be called when this window gets focus
        glfwSetWindowFocusCallback(m_Window,
            [](GLFWwindow* window, int focus) -> void {
                const MainWindow* data{ static_cast<MainWindow *>(glfwGetWindowUserPointer(window)) };
                if (focus == GLFW_TRUE) {
                    InputManager::SetFocus(data);
                }
            });
    }

    auto MainWindow::BeginFrame() -> void {

    }

    auto MainWindow::EndFrame() -> void {

    }


    auto MainWindow::SpawnOnCenter() const -> void {
#if !defined(NDEBUG)
        Int32_T count{};
        MKT_CORE_LOGGER_INFO("Number of available monitors: {}", count);
#endif
        // See: https://www.glfw.org/docs/3.3/monitor_guide.html
        // The primary monitor is returned by glfwGetPrimaryMonitor. It is the user's
        // preferred monitor and is usually the one with global UI elements like task bar or menu bar.
        Int32_T monitorWidth{};
        Int32_T monitorHeight{};
        GLFWmonitor* primary{ glfwGetPrimaryMonitor() };
        glfwGetMonitorWorkarea(primary, nullptr, nullptr, &monitorWidth, &monitorHeight);
        glfwSetWindowPos(m_Window, monitorWidth / 10, monitorHeight / 10);
    }

    auto MainWindow::InitGLFW() -> void {
        if (!s_GLFWInitSuccess) {
            auto ret{ glfwInit() };
            MKT_ASSERT(ret == GLFW_TRUE, "Failed to initialized the GLFW library");

            s_GLFWInitSuccess = true;
            glfwSetErrorCallback([](Int32_T errCode, const char* desc) -> void {
                MKT_CORE_LOGGER_ERROR("GLFW error code: {} Description: {}", errCode, desc);
                }
            );
        }
    }

    auto MainWindow::CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface) -> void {
        if (glfwCreateWindowSurface(instance, m_Window, nullptr, surface) != VK_SUCCESS)
            throw std::runtime_error("Failed to create Vulkan Surface");
    }

    auto MainWindow::ProcessEvents() -> void {
        glfwPollEvents();
    }

    auto MainWindow::DestroyGLFWWindow(GLFWwindow* window) -> void {
        // Everytime we shut down a GLFW window, we decrease the number
        // of active windows, the last GLFW window to be shutdown calls glfwTerminate()
        glfwDestroyWindow(window);
        // TODO: thread safety
        s_WindowsCount -= 1;

        if (s_WindowsCount == 0) {
            glfwTerminate();
        }
    }

    auto MainWindow::CreateGLFWWindow(const MainWindow::GLFWWindowCreateSpec &spec) -> GLFWwindow* {
        // TODO: thread safety?

        GLFWwindow* window{ nullptr };
        window = glfwCreateWindow(spec.Width, spec.Height, spec.Title.data(), nullptr, nullptr);
        s_WindowsCount += 1;

        return window;
    }
}