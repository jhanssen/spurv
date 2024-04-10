#include <Window.h>
#include "GlfwUserData.h"

using namespace spurv;

void Window::init_sys()
{
    if (isMainWindow()) {
        glfwInit();
    }
    mWindow = glfwCreateWindow(mWidth, mHeight, "Spurv", nullptr, nullptr);
    if (mWindow != nullptr) {
        auto userData = new GlfwUserData;
        userData->set<0>(this);
        glfwSetWindowUserPointer(mWindow, userData);
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
