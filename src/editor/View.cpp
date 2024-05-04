#include "View.h"
#include <Logger.h>
#include <Renderer.h>
#include <ScriptClass.h>
#include <ScriptEngine.h>

using namespace spurv;


View::View()
{
    setSelector("view");
}

View::~View()
{
}

void View::processDocument()
{
    // reset view to top, and make the renderer start churning on the data
    mFirstLine = 0;

    auto renderer = Renderer::instance();
    renderer->setPropertyFloat("foobar", Renderer::Property::FirstLine, static_cast<float>(mFirstLine));
    renderer->clearTextProperties("foobar");
    spdlog::info("view process doc {}", mDocument->numLines());
    if (mDocument->numLines() == 0) {
        // no text
        renderer->clearTextLines("foobar");
    } else {
        auto textLines = mDocument->textForRange(0, std::min<std::size_t>(mDocument->numLines(), 500));
        renderer->addTextLines("foobar", std::move(textLines));

        // start animating the first line just for shits and giggles
        // auto loop = EventLoop::eventLoop();
        // int32_t from = 0, to = 5;
        // loop->startTimer([renderer, from, to](uint32_t) mutable -> void {
        //     // spdlog::info("start anim {} {}", from, to);
        //     renderer->animatePropertyFloat(0, Renderer::Property::FirstLine, static_cast<float>(to), 1000, Ease::InOutQuad);

        //     if (from == 0) {
        //         from = 5;
        //         to = 0;
        //     } else {
        //         from = 0;
        //         to = 5;
        //     }
        // }, 2000, EventLoop::TimerMode::Repeat);
    }
}

void View::setDocument(const std::shared_ptr<Document>& doc)
{
    if (mDocument) {
        removeStyleableChild(mDocument.get());
    }
    mDocument = doc;
    if (mDocument) {
        addStyleableChild(mDocument.get());
    }

    if (mDocument->isReady()) {
        processDocument();
    } else {
        mDocument->onReady().connect([this]() {
            processDocument();
        });
    }
}

void View::updateLayout(const Rect& rect)
{
    (void)rect;
}
