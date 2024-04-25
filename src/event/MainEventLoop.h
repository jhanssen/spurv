#pragma once

#include "EventLoop.h"
#include "EventEmitter.h"

namespace spurv {

class MainEventLoop : public EventLoop
{
public:
    MainEventLoop();
    virtual ~MainEventLoop() override;

    virtual int run() override;

    EventEmitter<void(uint32_t)>& onUnicode();
    /**
     * glfwSetKeyCallback(GLFWwindow*, int key, int scancode, int action, int mods)
     */

    EventEmitter<void(int, int, int, int)>& onKey();

private:
    EventEmitter<void(uint32_t)> mOnUnicode;
    EventEmitter<void(int, int, int, int)> mOnKey;
};

inline EventEmitter<void(uint32_t)>& MainEventLoop::onUnicode()
{
    return mOnUnicode;
}

inline EventEmitter<void(int, int, int, int)>& MainEventLoop::onKey()
{
    return mOnKey;
}
} // namespace spurv
