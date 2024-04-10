#include "Window.h"

using namespace spurv;

Window* Window::sMainWindow = nullptr;

Window::Window(int32_t x, int32_t y, uint32_t width, uint32_t height)
    : mX(x), mY(y), mWidth(width), mHeight(height)
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
