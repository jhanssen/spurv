#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <config.h>
#include <Geometry.h>
#include <EventEmitter.h>
#include <EventLoop.h>
#include <volk.h>

#if defined(USE_GLFW)
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

    const Rect& rect() const;

    bool isMainWindow() const;
    static Window* mainWindow();

    EventLoop* eventLoop() const;
    VkSurfaceKHR surface(VkInstance instance);

    const SizeF& contentScale() const;

#if defined(USE_GLFW)
    GLFWwindow* glfwWindow() const;
#endif

    EventEmitter<void(int32_t, int32_t)>& onMove();
    EventEmitter<void(uint32_t, uint32_t)>& onResize();

private:
    void init_sys();
    void cleanup_sys();

private:
    mutable std::mutex mMutex;
    Rect mRect = {};
    SizeF mContentScale = {};
    EventEmitter<void(int32_t, int32_t)> mOnMove;
    EventEmitter<void(uint32_t, uint32_t)> mOnResize;

#if defined(USE_GLFW)
    GLFWwindow* mWindow = nullptr;
#endif

private:
    static Window* sMainWindow;

    VkSurfaceKHR mSurface = VK_NULL_HANDLE;
};

inline const Rect& Window::rect() const
{
    std::lock_guard lock(mMutex);
    return mRect;
}

inline const SizeF& Window::contentScale() const
{
    std::lock_guard lock(mMutex);
    return mContentScale;
}

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

inline EventEmitter<void(uint32_t, uint32_t)>& Window::onResize()
{
    return mOnResize;
}

inline EventEmitter<void(int32_t, int32_t)>& Window::onMove()
{
    return mOnMove;
}

} // namespace spurv
