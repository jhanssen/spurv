#include "View.h"
#include <Logger.h>
#include <Renderer.h>
#include <ScriptClass.h>
#include <ScriptEngine.h>
#include <Formatting.h>

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

    const uint64_t nm = frameNo();
    auto renderer = Renderer::instance();
    renderer->setPropertyFloat(nm, Renderer::Property::FirstLine, static_cast<float>(mFirstLine));
    renderer->clearTextProperties(nm);
    spdlog::info("view process doc {}", mDocument->numLines());
    if (mDocument->numLines() == 0) {
        // no text
        renderer->clearTextLines(nm);
    } else {
        auto textLines = mDocument->textForRange(0, std::min<std::size_t>(mDocument->numLines(), 500));
        renderer->addTextLines(nm, std::move(textLines));

        auto props = mDocument->propertiesForRange(0, std::min<std::size_t>(mDocument->numLines(), 500));
        for (auto& prop : props) {
            spdlog::debug("prop {}-{}, color {}", prop.start, prop.end, prop.foreground);
        }
        renderer->addTextProperties(nm, std::move(props));

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
        mDocument->onPropertiesChanged().disconnectAll();
    }

    auto oldDocument = mDocument;

    mDocument = doc;
    if (mDocument) {
        addStyleableChild(mDocument.get());
        mDocument->onPropertiesChanged().connect([this](std::size_t start, std::size_t end) {
            auto props = mDocument->propertiesForRange(start, end);
            for (auto& prop : props) {
                spdlog::debug("updated prop {}-{}, color {}", prop.start, prop.end, prop.foreground);
            }
            auto renderer = Renderer::instance();
            renderer->addTextProperties(frameNo(), std::move(props));
        });
    }

    if (mDocument->isReady()) {
        processDocument();
    } else {
        mDocument->onReady().connect([this]() {
            processDocument();
        });
    }

    mOnDocumentChanged.emit(oldDocument);
}

void View::updateLayout(const Rect& rect)
{
    Frame::updateLayout(rect);
}
