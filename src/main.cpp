#include "text/Font.h"
#include "window/Window.h"
#include "event/EventLoop.h"
#include "Args.h"
#include <fmt/core.h>

using namespace spurv;

int main(int argc, char** argv, char** envp)
{
    const auto args = ArgsParser::parse(argc, argv, envp, "SPURV_", [](const char* msg, size_t offset, char* arg) {
        fmt::print(stderr, "{}: {} ({})\n", msg, offset, arg);
        exit(1);
    });

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
