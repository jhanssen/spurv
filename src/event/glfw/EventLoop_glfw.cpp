#include <EventLoop.h>
#include <window/Window.h>
#include <window/glfw/GlfwUserData.h>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <cassert>

using namespace spurv;

void EventLoop::run()
{
    auto mainWindow = Window::mainWindow()->glfwWindow();
    if (!mainWindow)
        return;

    GlfwUserData* userData = reinterpret_cast<GlfwUserData*>(glfwGetWindowUserPointer(mainWindow));
    assert(userData != nullptr);
    userData->set<1>(this);

    glfwSetCharCallback(mainWindow, [](GLFWwindow* win, unsigned int codepoint) {
        auto eventLoop = GlfwUserData::get<1, EventLoop>(win);
        eventLoop->mOnUnicode.emit(codepoint);
    });

    for (;;) {
        if (!run_internal())
            break;
        glfwWaitEvents();
    }
}

void EventLoop::stop()
{
    stop_internal();
    glfwPostEmptyEvent();
}

void EventLoop::post(std::unique_ptr<Event>&& event)
{
    post_internal(std::move(event));
    glfwPostEmptyEvent();
}
