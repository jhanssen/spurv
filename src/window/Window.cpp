#include "Window.h"
#include <EventLoop.h>

using namespace spurv;

Window* Window::sMainWindow = nullptr;

Window::Window(const Rect& rect)
    : mRect(rect)
{
    if (sMainWindow == nullptr) {
        sMainWindow = this;
    }
    init_sys();
}

Window::~Window()
{
    cleanup_sys();
    if (sMainWindow == this) {
        sMainWindow = nullptr;
    }
}

EventLoop* Window::eventLoop() const
{
    // I guess windows should only be created from the main thread?
    return EventLoop::mainEventLoop();
}
