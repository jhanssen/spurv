#include <Window.h>
#include "GlfwUserData.h"

using namespace spurv;

void Window::init_sys()
{
    if (isMainWindow()) {
        glfwInit();
    }
    mWindow = glfwCreateWindow(mRect.width, mRect.height, "Spurv", nullptr, nullptr);
    if (mWindow != nullptr) {
        glfwSetWindowPos(mWindow, mRect.x, mRect.y);

        auto userData = new GlfwUserData;
        userData->set<0>(this);
        glfwSetWindowUserPointer(mWindow, userData);

        glfwSetWindowPosCallback(mWindow, [](GLFWwindow* glfwwin, int x, int y) {
            auto window = GlfwUserData::get<0, Window>(glfwwin);
            window->mRect.x = x;
            window->mRect.y = y;

            window->mOnMove.emit(x, y);
        });
        glfwSetWindowSizeCallback(mWindow, [](GLFWwindow* glfwwin, int w, int h) {
            auto window = GlfwUserData::get<0, Window>(glfwwin);
            window->mRect.width = w;
            window->mRect.height = h;

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
