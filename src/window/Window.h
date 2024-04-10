#pragma once

#include <cstdint>
#include <memory>
#include <config.h>

#if defined(USE_GLFW)
# include <vulkan/vulkan.h>
# include <GLFW/glfw3.h>
#endif

namespace spurv {

class Window
{
public:
    Window(int32_t x, int32_t y, uint32_t width, uint32_t height);
    ~Window();

    void show();
    void hide();

    bool isMainWindow() const;

    static Window* mainWindow();

#if defined(USE_GLFW)
    GLFWwindow* glfwWindow() const;
#endif

private:
    int32_t mX, mY;
    uint32_t mWidth, mHeight;

private:
    void init_sys();
    void cleanup_sys();

#if defined(USE_GLFW)
    GLFWwindow* mWindow = nullptr;
#endif

private:
    static Window* sMainWindow;
};

inline bool Window::isMainWindow() const
{
    return sMainWindow == this;
}

inline Window* Window::mainWindow()
{
    return sMainWindow;
}

#if defined(USE_GLFW)
inline GLFWwindow* Window::glfwWindow() const
{
    return mWindow;
}
#endif

} // namespace spurv
