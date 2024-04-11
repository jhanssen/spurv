#include "text/Font.h"
#include "window/Window.h"
#include "event/EventLoop.h"
#include "Args.h"
#include <fmt/core.h>
#include <cstdint>

using namespace spurv;

int main(int argc, char** argv, char** envp)
{
    const auto args = ArgsParser::parse(argc, argv, envp, "SPURV_", [](const char* msg, size_t offset, char* arg) {
        fmt::print(stderr, "Spurv -- {}: {} ({})\n", msg, offset, arg);
        ::exit(1);
    });

    Font font("Corsiva");
    fmt::print("hello world {}\n", font.file().string());

    const auto x = args.value<int32_t>("x", 0);
    const auto y = args.value<int32_t>("y", 0);
    const auto width = args.value<uint32_t>("width", 1920);
    const auto height = args.value<uint32_t>("height", 1920);

    Window window(x, y, width, height);
    window.show();

    EventLoop loop;
    loop.onUnicode().connect([](uint32_t uc) {
        fmt::print("unicode {}\n", uc);
    });
    loop.run();
}
