#pragma once

#include <cstdint>
#include <memory>
#include <config.h>
#include <Geometry.h>
#include <Signal.h>

#if defined(USE_GLFW)
# include <vulkan/vulkan.h>
# include <GLFW/glfw3.h>
#endif

namespace spurv {

class Window
{
public:
    Window(const Rect& rect);
    ~Window();

    void show();
    void hide();

    bool isMainWindow() const;

    const Rect& rect() const;

    static Window* mainWindow();

#if defined(USE_GLFW)
    GLFWwindow* glfwWindow() const;
#endif

    Signal<void(int32_t, int32_t)>& onMove();
    Signal<void(uint32_t, uint32_t)>& onResize();

private:
    void init_sys();
    void cleanup_sys();

private:
    Rect mRect;
    Signal<void(int32_t, int32_t)> mOnMove;
    Signal<void(uint32_t, uint32_t)> mOnResize;

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

inline Signal<void(uint32_t, uint32_t)>& Window::onResize()
{
    return mOnResize;
}

inline Signal<void(int32_t, int32_t)>& Window::onMove()
{
    return mOnMove;
}

} // namespace spurv
