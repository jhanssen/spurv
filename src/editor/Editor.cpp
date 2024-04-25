#include "Editor.h"
#include <Logger.h>
#include <MainEventLoop.h>
#include <Renderer.h>
#include <ScriptEngine.h>
#include <EventLoopUv.h>
#include <fmt/core.h>
#include <uv.h>
#include <Thread.h>

using namespace spurv;

namespace spurv {
struct EditorImpl
{
    uv_loop_t* loop = nullptr;
    std::filesystem::path appPath;
};
} // namespace spurv

std::unique_ptr<Editor> Editor::sInstance = {};

Editor::Editor(const std::filesystem::path &appPath)
    : mEventLoop(new EventLoopUv())
{
    mImpl = new EditorImpl();
    mImpl->appPath = appPath;
    mThread = std::thread(&Editor::thread_internal, this);
}

Editor::~Editor()
{
    delete mImpl;
}

void Editor::thread_internal()
{
    setCurrentThreadName("Editor");
    // finalize the event loop
    mEventLoop->install();
    mEventLoop->post([this]() {
        {
            std::unique_lock lock(mMutex);
            mImpl->loop = static_cast<uv_loop_t*>(mEventLoop->handle());

            mInitialized = true;
        }

        auto mainEventLoop = static_cast<MainEventLoop*>(EventLoop::mainEventLoop());
        mScriptEngine = std::make_unique<ScriptEngine>(mEventLoop.get(), mImpl->appPath);
        mScriptEngine->onExit().connect([this, mainEventLoop](int exitCode) {
            mEventLoop->stop(0);
            mainEventLoop->post([mainEventLoop, exitCode]() {
                mainEventLoop->stop(exitCode);
            });
        });
        mainEventLoop->post([editor = this]() {
            editor->mOnReady.emit();
        });
        mConnectKeys[0] = mainEventLoop->onKey().connect([this](int key, int scancode, int action, int mods) {
            mScriptEngine->onKey(key, scancode, action, mods);
        });
        mConnectKeys[1] = mainEventLoop->onUnicode().connect([this](uint32_t uc) {
            spdlog::error("editor uc {}", uc);
            if (mCurrentDoc) {
                mCurrentDoc->insert(static_cast<char32_t>(uc));
            }
        });

    });
    mEventLoop->run();

    if (mInitialized) {
        auto mainEventLoop = static_cast<MainEventLoop*>(EventLoop::mainEventLoop());
        mainEventLoop->onKey().disconnect(mConnectKeys[0]);
        mainEventLoop->onUnicode().disconnect(mConnectKeys[1]);
    }

    mEventLoop->uninstall();
    {
        std::unique_lock lock(mMutex);
        mEventLoop.reset();
        mScriptEngine.reset();
        mDocuments.clear();
        mCurrentDoc = nullptr;
        mCond.notify_one();
    }
}

void Editor::stop()
{
    {
        std::unique_lock lock(mMutex);
        assert(mInitialized);
        while (mEventLoop) {
            mCond.wait(lock);
        }
    }
    mThread.join();
}

void Editor::initialize(const std::filesystem::path &appPath)
{
    assert(!sInstance);
    sInstance = std::unique_ptr<Editor>(new Editor(appPath));
}

void Editor::load(const std::filesystem::path& path)
{
    if (path.empty()) {
        return;
    }
    mEventLoop->post([this, path]() {
        if (mDocuments.empty()) {
            assert(mCurrentDoc == nullptr);
            mDocuments.push_back(std::make_unique<Document>());
            mCurrentDoc = mDocuments.back().get();

            mCurrentDoc->setFont(Font("Inconsolata", 25));

            mCurrentDoc->setStylesheet("hello1 { color: #ff4354 }\nhello4 { color: #0000ff }");
            auto selector1 = mCurrentDoc->addSelector(50, 100, "hello1");
            auto selector2 = mCurrentDoc->addSelector(75, 95, "hello2");
            auto selector3 = mCurrentDoc->addSelector(75, 150, "hello3");
            auto selector4 = mCurrentDoc->addSelector(160, 170, "hello4");

            mCurrentDoc->onReady().connect([
                doc = mCurrentDoc,
                selector1 = std::move(selector1),
                selector2 = std::move(selector2),
                selector3 = std::move(selector3),
                selector4 = std::move(selector4)]() {
                spdlog::info("document ready");
                // send some lines to the renderer for now
                std::size_t lineNo = 0;
                auto renderer = Renderer::instance();
                auto textLines = doc->textForRange(lineNo, lineNo + 100);
                renderer->addTextLines(0, std::move(textLines));
                auto props = doc->propertiesForRange(lineNo, lineNo + 100);
                for (auto& prop : props) {
                    spdlog::debug("prop {}-{}, color {}", prop.start, prop.end, prop.foreground);
                }
                renderer->addTextProperties(0, std::move(props));
            });
        }
        mDocuments.back()->load(path);
    });
}
