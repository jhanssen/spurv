#include <Window.h>
#include <Logger.h>
#include "GlfwUserData.h"
#include <fmt/core.h>

using namespace spurv;

void Window::init_sys()
{
    if (isMainWindow()) {
        glfwInitVulkanLoader(vkGetInstanceProcAddr);
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }
    Rect rect;
    {
        std::lock_guard lock(mMutex);
        rect = mRect;
    }
    mWindow = glfwCreateWindow(rect.width, rect.height, "Spurv", nullptr, nullptr);
    if (mWindow != nullptr) {
        glfwSetWindowPos(mWindow, rect.x, rect.y);

        auto userData = new GlfwUserData;
        userData->set<0>(this);
        glfwSetWindowUserPointer(mWindow, userData);

        glfwSetWindowPosCallback(mWindow, [](GLFWwindow* glfwwin, int x, int y) {
            auto window = GlfwUserData::get<0, Window>(glfwwin);
            {
                std::lock_guard lock(window->mMutex);
                window->mRect.x = x;
                window->mRect.y = y;
            }

            window->mOnMove.emit(x, y);
        });
        glfwSetWindowSizeCallback(mWindow, [](GLFWwindow* glfwwin, int w, int h) {
            auto window = GlfwUserData::get<0, Window>(glfwwin);
            {
                std::lock_guard lock(window->mMutex);
                window->mRect.width = w;
                window->mRect.height = h;
            }

            window->mOnResize.emit(w, h);
        });
    }
}

void Window::cleanup_sys()
{
    if (mWindow != nullptr) {
        glfwDestroyWindow(mWindow);
    }
    if (isMainWindow()) {
        glfwTerminate();
    }
}

void Window::show()
{
    if (mWindow == nullptr) {
        return;
    }
    glfwShowWindow(mWindow);
}

void Window::hide()
{
    if (mWindow == nullptr) {
        return;
    }
    glfwHideWindow(mWindow);
}

VkSurfaceKHR Window::surface(VkInstance instance)
{
    if (mSurface == VK_NULL_HANDLE && mWindow) {
        VkResult err = glfwCreateWindowSurface(instance, mWindow, nullptr, &mSurface);
        if (err) {
            spdlog::error("Unable to create window surface: {}", err);
            mSurface = VK_NULL_HANDLE; // just in case
        }
    }
    return mSurface;
}
