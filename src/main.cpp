#include "text/Font.h"
#include "window/Window.h"
#include "event/EventLoop.h"
#include <fmt/core.h>

using namespace spurv;

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    Font font("Corsiva");
    fmt::print("hello world {}\n", font.file().string());

    Window window(0, 0, 1920, 1080);
    window.show();

    EventLoop loop;
    loop.onUnicode().connect([](uint32_t uc) {
        fmt::print("unicode {}\n", uc);
    });
    loop.run();
}
