#include "View.h"
#include <Renderer.h>
#include <spdlog/spdlog.h>

using namespace spurv;

View::View(uint32_t id)
    : mViewId(id)
{
}

View::~View()
{
}

void View::processDocument()
{
    // reset view to top, and make the renderer start churning on the data
    mFirstLine = 0;

    auto renderer = Renderer::instance();
    renderer->setPropertyFloat(mViewId, Renderer::Property::FirstLine, static_cast<float>(mFirstLine));
    renderer->clearTextProperties(mViewId);
    spdlog::info("view process doc {}", mDocument->numLines());
    if (mDocument->numLines() == 0) {
        // no text
        renderer->clearTextLines(mViewId);
    } else {
        auto textLines = mDocument->textForRange(0, std::min<std::size_t>(mDocument->numLines(), 500));
        renderer->addTextLines(mViewId, std::move(textLines));

        // start animating the first line just for shits and giggles
        auto loop = EventLoop::eventLoop();
        int32_t from = 0, to = 5;
        loop->startTimer([this, renderer, from, to](uint32_t) mutable -> void {
            // spdlog::info("start anim {} {}", from, to);
            renderer->animatePropertyFloat(mViewId, Renderer::Property::FirstLine, static_cast<float>(to), 1000, Ease::InOutQuad);

            if (from == 0) {
                from = 5;
                to = 0;
            } else {
                from = 0;
                to = 5;
            }
        }, 2000, EventLoop::TimerMode::Repeat);
    }
}

void View::setDocument(const std::shared_ptr<Document>& doc)
{
    mDocument = doc;
    if (mDocument->isReady()) {
        processDocument();
    } else {
        mDocument->onReady().connect([this]() {
            processDocument();
        });
    }
}
