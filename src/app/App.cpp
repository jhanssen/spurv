#include "App.h"
#include "text/Font.h"
#include "window/Window.h"
#include "event/MainEventLoop.h"
#include "Args.h"
#include "AppPath.h"
#include "common/Geometry.h"
#include "thread/ThreadPool.h"
#include "editor/Editor.h"
#include "render/Renderer.h"
#include <fmt/core.h>
#include <Logger.h>
#include <cstdint>
#include <uv.h>

namespace spurv {
int app(int argc, char** argv, char** envp)
{
    uv_disable_stdio_inheritance();
    const auto args = ArgsParser::parse(argc, argv, envp, "SPURV_", [](const char* msg, size_t offset, char* arg) {
        fmt::print(stderr, "Spurv -- {}: {} ({})\n", msg, offset, arg);
        ::exit(1);
    });

    const auto level = args.value<std::string>("log-level", "info");
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
        fmt::print(stderr, "Spurv -- invalid log level {}\n", level);
        ::exit(1);
    }

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

    std::filesystem::path self = appPath();

    MainEventLoop loop;

    Renderer::initialize(self);
    Renderer::instance()->onReady().connect([filename = std::move(filename), self = std::move(self), argc, argv, envp]() {
        spdlog::info("renderer ready");

        Editor::initialize(self, argc, argv, envp);
        // Editor::instance()->onReady().connect([filename = std::move(filename)]() {
        //     spdlog::info("editor ready");
        //     Editor::instance()->load(std::filesystem::path(filename));
        // });
    });
    const int32_t ret = loop.run();
    ThreadPool::destroyMainThreadPool();
    Editor::destroy();
    Renderer::destroy();
    return ret;
}
} // namespace spurv
