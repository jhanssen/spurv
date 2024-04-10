#pragma once

#include <array>
#include <cstddef>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

namespace spurv {

class GlfwUserData
{
public:
    template<std::size_t Idx, typename T>
    T* get() const;

    template<std::size_t Idx, typename T>
    void set(T* t);

    template<std::size_t Idx, typename T>
    static T* get(GLFWwindow *win);

private:
    enum { NumEntries = 4 };

    std::array<void*, NumEntries> mTs = {};
};

template<std::size_t Idx, typename T>
T* GlfwUserData::get() const
{
    static_assert(Idx < NumEntries);
    return reinterpret_cast<T*>(mTs[Idx]);
}

template<std::size_t Idx, typename T>
void GlfwUserData::set(T* t)
{
    static_assert(Idx < NumEntries);
    mTs[Idx] = t;
}

template<std::size_t Idx, typename T>
T* GlfwUserData::get(GLFWwindow *win)
{
    auto userData = reinterpret_cast<GlfwUserData*>(glfwGetWindowUserPointer(win));
    return userData->get<Idx, T>();
}

} // namespace spurv
