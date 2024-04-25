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
