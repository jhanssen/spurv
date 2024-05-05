#include "Editor.h"
#include "View.h"
#include <Logger.h>
#include <MainEventLoop.h>
#include <Renderer.h>
#include <ScriptEngine.h>
#include <Window.h>
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

class ViewInstance : public ScriptClassInstance
{
public:
    std::shared_ptr<View> view {};
    int32_t pos { 0 };
};

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

    auto window = Window::mainWindow();
    if (window == nullptr) {
        spdlog::critical("No window created");
        return;
    }

    // finalize the event loop
    mEventLoop->install();
    mEventLoop->post([this, window]() {
        {
            std::unique_lock lock(mMutex);
            mImpl->loop = static_cast<uv_loop_t*>(mEventLoop->handle());

            mInitialized = true;
        }

        auto mainEventLoop = static_cast<MainEventLoop*>(EventLoop::mainEventLoop());
        mScriptEngine = std::make_unique<ScriptEngine>(mEventLoop.get(), mImpl->appPath);
        ScriptClass clazz("View",
                          [this](ScriptClassInstance *&instance, std::vector<ScriptValue> &&/*args*/) -> std::optional<std::string> {
                              ViewInstance *v = new ViewInstance;
                              v->view = mCurrentView;
                              instance = v;
                              return {};
                          });

        clazz.addMethod("scrollDown", [](ScriptClassInstance *instance, std::vector<ScriptValue> &&) -> ScriptValue {
            ViewInstance *v = static_cast<ViewInstance *>(instance);
            auto renderer = Renderer::instance();
            renderer->animatePropertyFloat(v->view->name(), Renderer::Property::FirstLine, static_cast<float>(++v->pos), 100, Ease::InOutQuad);
            return {};
        });


        clazz.addMethod("scrollUp", [](ScriptClassInstance *instance, std::vector<ScriptValue> &&) -> ScriptValue {
            ViewInstance *v = static_cast<ViewInstance *>(instance);
            if (v->pos > 0) {
                auto renderer = Renderer::instance();
                renderer->animatePropertyFloat(v->view->name(), Renderer::Property::FirstLine, static_cast<float>(--v->pos), 100, Ease::InOutQuad);
            }
            return {};
        });
        clazz.addProperty("currentLine", [](ScriptClassInstance *instance) -> ScriptValue {
            ViewInstance *v = static_cast<ViewInstance *>(instance);
            return ScriptValue(v->pos);
        });
        ScriptEngine::scriptEngine()->addClass(std::move(clazz));

        mScriptEngine->onExit().connect([this, mainEventLoop](int exitCode) {
            mEventLoop->stop(0);
            mainEventLoop->post([mainEventLoop, exitCode]() {
                mainEventLoop->stop(exitCode);
            });
        });
        mainEventLoop->post([editor = this]() {
            editor->mOnReady.emit();
        });
        mConnectKeys[0] = mainEventLoop->onKey().connect([this](int key, int scancode, int action, int mods, const std::optional<std::string> &keyName) {
            mScriptEngine->onKey(key, scancode, action, mods, keyName);
        });
        mConnectKeys[1] = mainEventLoop->onUnicode().connect([](uint32_t uc) {
            spdlog::error("editor uc {}", uc);
        });
        mConnectKeys[2] = window->onResize().connect([this](uint32_t w, uint32_t h) {
            mWidth = w;
            mHeight = h;

            relayout();
        });
        const auto& windowRect = window->rect();
        mWidth = windowRect.width;
        mHeight = windowRect.height;

    });
    mEventLoop->run();

    if (mInitialized) {
        auto mainEventLoop = static_cast<MainEventLoop*>(EventLoop::mainEventLoop());
        mainEventLoop->onKey().disconnect(mConnectKeys[0]);
        mainEventLoop->onUnicode().disconnect(mConnectKeys[1]);
        window->onResize().disconnect(mConnectKeys[2]);
    }

    mEventLoop->uninstall();
    {
        std::unique_lock lock(mMutex);
        mEventLoop.reset();
        mScriptEngine.reset();
        mDocuments.clear();
        mCurrentView = nullptr;
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
        mContainer = std::make_unique<Container>();
        mContainer->makeRoot();
        // addStyleableChild(mContainer.get());

        mContainer->setSelector("editor");
        mContainer->mutableSelector()[0].id(mName);
        mContainer->setStylesheet(mQss);

        if (mDocuments.empty()) {
            assert(mCurrentView == nullptr);
            mDocuments.push_back(std::make_shared<Document>());
            auto view = std::make_shared<View>();
            mContainer->addFrame(view);
            mContainer->addClass("border");
            view->setName("hello");
            view->setDocument(mDocuments.back());
            view->setActive(false);
            mCurrentView = view;
            auto currentDoc = mCurrentView->document().get();
            currentDoc->setName("doccy");

            // currentDoc->matchesSelector("editor document");
            // view->matchesSelector("container > view#hello:!active");

            mContainer->setStylesheet(
                "editor { flex-direction: column;flex: 1; }\n"
                "container.border { border: 10 #229922; }\n"
                "editor > view { flex: 1; }\n"
                "editor document { flex: 1; margin: 5   2; }");

            currentDoc->setFont(Font("Inconsolata", 25));

            relayout();

            /*
            currentDoc->setStylesheet("hello1 { color: #ff4354 }\nhello4 { color: #0000ff }");
            auto selector1 = currentDoc->addSelector(50, 100, "hello1");
            auto selector2 = currentDoc->addSelector(75, 95, "hello2");
            auto selector3 = currentDoc->addSelector(75, 150, "hello3");
            auto selector4 = currentDoc->addSelector(160, 170, "hello4");

            currentDoc->onReady().connect([
                doc = currentDoc,
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
            */
        }
        mDocuments.back()->load(path);
    });
}

void Editor::setName(const std::string& name)
{
    mName = name;
    if (mContainer) {
        mContainer->mutableSelector()[0].id(name);
    }
}

void Editor::setStylesheet(const qss::Document& qss, StylesheetMode mode)
{
    if (mode == StylesheetMode::Replace) {
        mQss = qss;
    } else {
        mQss += qss;
    }
    if (mContainer) {
        mContainer->setStylesheet(mQss);
    }
}

void Editor::setStylesheet(const std::string& qss, StylesheetMode mode)
{
    setStylesheet(qss::Document(qss), mode);
}

void Editor::relayout()
{
    auto& root = mContainer;

    YGNodeStyleSetWidth(root->mYogaNode, mWidth);
    YGNodeStyleSetHeight(root->mYogaNode, mHeight);

    YGNodeCalculateLayout(root->mYogaNode, YGUndefined, YGUndefined, YGDirectionLTR);

    root->relayout();
}
