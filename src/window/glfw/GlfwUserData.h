#pragma once

#include <array>
#include <cstddef>
#include <volk.h>
#include <GLFW/glfw3.h>
#include <cassert>

namespace spurv {

class GlfwUserData
{
public:
    template<std::size_t Idx, typename T>
    T* get() const;

    template<std::size_t Idx, typename T>
    void set(T* t);

    template<std::size_t Idx, typename T>
    static T* get(GLFWwindow* win);

    template<std::size_t Idx, typename T>
    static void set(GLFWwindow* win, T* t);

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
    auto userData = static_cast<GlfwUserData*>(glfwGetWindowUserPointer(win));
    assert(userData != nullptr);
    return userData->get<Idx, T>();
}

template<std::size_t Idx, typename T>
void GlfwUserData::set(GLFWwindow* win, T* t)
{
    auto userData = static_cast<GlfwUserData*>(glfwGetWindowUserPointer(win));
    assert(userData != nullptr);
    userData->set<Idx>(t);
}

} // namespace spurv
