#include "Window.h"

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
