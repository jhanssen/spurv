#include "Frame.h"
#include <Logger.h>

using namespace spurv;

uint64_t Frame::sFrameNo = 0;

Frame::Frame()
{
    setSelector("frame");
    setName(fmt::format("spurv_frame_{}", sFrameNo++));
    onNameChanged().connect([this](const std::string& oldName) {
        auto renderer = Renderer::instance();
        renderer->renameIdentifier(oldName, name());
    });
    onAppliedStylesheet().connect([this]() {
        extractRenderViewData();
    });
}

void Frame::extractRenderViewData()
{
    const auto& bs = boxShadow();
    mRenderViewData.backgroundColor = { 0.3f, 0.3f, 0.3f, 0.3f };
    mRenderViewData.viewColor = color(ColorType::Background).value_or(Color {});
    mRenderViewData.borderColor = color(ColorType::Border).value_or(Color {});
    mRenderViewData.shadowColor = color(ColorType::Shadow).value_or(Color {});
    mRenderViewData.borderRadius = borderRadius();
    // ### should deal with separate border widths here
    mRenderViewData.borderThickness = static_cast<float>(border()[0]);
    mRenderViewData.edgeSoftness = 2.f;
    mRenderViewData.borderSoftness = 2.f;
    mRenderViewData.shadowOffset = {
        static_cast<uint32_t>(bs.hoffset),
        static_cast<uint32_t>(bs.voffset)
    };
    mRenderViewData.shadowSoftness = bs.blur;
    spdlog::debug("extracted render data bwidth {:.2f} bradius {} {}", mRenderViewData.borderThickness, mRenderViewData.borderRadius[0], mSelector.toString());
}

void Frame::updateLayout(const Rect& rect)
{
    mRenderViewData.frame = rect;
    mRenderViewData.content = rect;

    const auto& pd = padding();
    const auto& bw = border();
    mRenderViewData.content.x += bw[0] + pd[0];
    mRenderViewData.content.y += bw[1] + pd[0];
    if (bw[0] + bw[2] + pd[0] + pd[2] < mRenderViewData.content.width) {
        mRenderViewData.content.width -= (bw[0] + bw[2] + pd[0] + pd[2]);
    } else {
        mRenderViewData.content.width = 0;
    }
    if (bw[1] + bw[3] + pd[1] + pd[3] < mRenderViewData.content.height) {
        mRenderViewData.content.height -= (bw[1] + bw[3] + pd[1] + pd[3]);
    } else {
        mRenderViewData.content.height = 0;
    }

    auto renderer = Renderer::instance();
    spdlog::debug("setting render view data {}", name());
    renderer->setRenderViewData(name(), mRenderViewData);
}
