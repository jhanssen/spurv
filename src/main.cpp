#include "text/Font.h"
#include "window/Window.h"
#include "event/MainEventLoop.h"
#include "Args.h"
#include "common/Geometry.h"
#include "thread/ThreadPool.h"
#include "editor/Editor.h"
#include "render/Renderer.h"
#include <fmt/core.h>
#include <Logger.h>
#include <cstdint>

using namespace spurv;

int main(int argc, char** argv, char** envp)
{
    const auto args = ArgsParser::parse(argc, argv, envp, "SPURV_", [](const char* msg, size_t offset, char* arg) {
        fmt::print(stderr, "Spurv -- {}: {} ({})\n", msg, offset, arg);
        ::exit(1);
    });

    const auto level = args.value<std::string>("log-level", "error");
    if (level == "trace") {
        spdlog::set_level(spdlog::level::trace);
    } else if (level == "debug") {
        spdlog::set_level(spdlog::level::debug);
    } else if (level == "info" || level == "information") {
        spdlog::set_level(spdlog::level::info);
    } else if (level == "warn" || level == "warning") {
        spdlog::set_level(spdlog::level::warn);
    } else if (level == "err" || level == "error") {
        spdlog::set_level(spdlog::level::err);
    } else if (level == "critical") {
        spdlog::set_level(spdlog::level::critical);
    } else if (level == "off") {
        spdlog::set_level(spdlog::level::off);
    } else {
        fmt::print("Spurv -- invalid log level {}\n", level);
    }

    Font font("Corsiva");
    spdlog::info("hello world {}", font.file().string());

    const Rect rect = {
        .x = args.value<int32_t>("x", 0),
        .y = args.value<int32_t>("y", 0),
        .width = args.value<uint32_t>("width", 1920),
        .height = args.value<uint32_t>("height", 1080)
    };

    ThreadPool::initializeMainThreadPool();

    Window window(rect);
    window.show();

    std::string filename = args.freeformSize() > 0 ? args.freeformValue(0) : std::string {};

    MainEventLoop loop;

    Renderer::initialize();
    Renderer::instance()->onReady().connect([filename = std::move(filename)]() {
        spdlog::info("renderer ready");

        Editor::initialize();
        Editor::instance()->load(std::filesystem::path(filename));
        Editor::instance()->onReady().connect([]() {
            spdlog::info("editor ready");
        });
    });
    Renderer::instance()->setBoxes({ {
                Box {
                    { 0.8, 0.0, 0.8, 0.8 },
                    { 0.3, 0.0, 0.3, 0.3 }
                }
            } });
    loop.run();
}
