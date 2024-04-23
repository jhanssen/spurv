#include "Editor.h"
#include <Renderer.h>
#include <MainEventLoop.h>
#include <Logger.h>
#include <fmt/core.h>
#include <uv.h>

using namespace spurv;

namespace spurv {
struct EditorImpl
{
    uv_loop_t* loop = nullptr;
};
} // namespace spurv

std::unique_ptr<Editor> Editor::sInstance = {};

Editor::Editor()
    : mEventLoop(new EventLoop())
{
    mImpl = new EditorImpl();
    mThread = std::thread(&Editor::thread_internal, this);
}

Editor::~Editor()
{
    delete mImpl;
}

void Editor::thread_internal()
{
    // finalize the event loop
    mEventLoop->install();
    mEventLoop->post([this]() {
        {
            std::unique_lock lock(mMutex);
            mImpl->loop = static_cast<uv_loop_t*>(mEventLoop->handle());

            mInitialized = true;
        }

        auto loop = static_cast<MainEventLoop*>(EventLoop::mainEventLoop());
        loop->post([editor = this]() {
            editor->mOnReady.emit();
        });
        loop->onUnicode().connect([this](uint32_t uc) {
            spdlog::error("editor uc {}", uc);
            if (mCurrentDoc) {
                mCurrentDoc->insert(static_cast<char32_t>(uc));
            }
        });
    });
    mEventLoop->run();
}

void Editor::stop()
{
    {
        std::unique_lock lock(mMutex);
        assert(mInitialized);
    }
    mEventLoop->stop();
    mThread.join();
}

void Editor::initialize()
{
    assert(!sInstance);
    sInstance = std::unique_ptr<Editor>(new Editor());
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
