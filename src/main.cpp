#include "text/Font.h"
#include "window/Window.h"
#include "event/MainEventLoop.h"
#include "Args.h"
#include "common/Geometry.h"
#include "thread/ThreadPool.h"
#include "render/Renderer.h"
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

    const Rect rect = {
        .x = args.value<int32_t>("x", 0),
        .y = args.value<int32_t>("y", 0),
        .width = args.value<uint32_t>("width", 1920),
        .height = args.value<uint32_t>("height", 1080)
    };

    Window window(rect);
    window.show();

    MainEventLoop loop;
    loop.onUnicode().connect([](uint32_t uc) {
        fmt::print("unicode {}\n", uc);
    });

    Renderer::initialize();
    Renderer::instance()->setBoxes({ {
                Box {
                    { 0.8, 0.0, 0.8, 0.8 },
                    { 0.3, 0.0, 0.3, 0.3 }
                }
            } });
    loop.run();
}
