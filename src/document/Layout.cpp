#include "Layout.h"
#include <EventLoop.h>
#include <ThreadPool.h>
#include <mutex>
#include <cassert>
#include <cstring>
#include <spdlog/spdlog.h>

namespace spurv {
struct LayoutChunk
{
    std::u32string data;
    hb_font_t* font = nullptr;
};

class LayoutJob
{
public:
    std::mutex mutex;
    std::vector<LayoutChunk> chunks;
    std::vector<Linebreak> linebreaks;
    std::size_t length = 0;
    bool running = false;
    Layout* layout = nullptr;

    static void runJob(std::shared_ptr<LayoutJob> job);
};

void LayoutJob::runJob(std::shared_ptr<LayoutJob> job)
{
    job->running = true;
    auto loop = EventLoop::eventLoop();
    ThreadPool::mainThreadPool()->post([job = std::move(job), loop]() -> void {
        std::size_t processed = 0, processedStart = 0;
        std::size_t currentChunk = 0;
        std::size_t currentLinebreak = 0;
        LayoutChunk chunk;
        std::vector<Linebreak> linebreaks;
        std::vector<Layout::LineInfo> buffers;

        for (;; ++currentChunk) {
            {
                std::lock_guard lock(job->mutex);
                if (!buffers.empty()) {
                    loop->post([buffers = std::move(buffers), layout = job->layout]() -> void {
                        layout->mLines.reserve(layout->mLines.size() + buffers.size());
                        layout->mLines.insert(layout->mLines.end(), buffers.begin(), buffers.end());
                        layout->notifyLines();
                    });
                }
                if (currentChunk == job->chunks.size()) {
                    job->running = false;
                    return;
                }
                // take this chunk
                if (chunk.font != nullptr && chunk.font == job->chunks[currentChunk].font) {
                    // this is bad, we have left over data but this new chunk
                    // has a different font. so discard the previous data
                    hb_font_destroy(chunk.font);
                    chunk.data.clear();
                }
                chunk.font = job->chunks[currentChunk].font;
                if (chunk.data.empty()) {
                    chunk.data = std::move(job->chunks[currentChunk].data);
                    processed += chunk.data.size();
                } else {
                    chunk.data += job->chunks[currentChunk].data;
                    processed += job->chunks[currentChunk].data.size();
                }
                processedStart = processed - chunk.data.size();
                // copy the linebreaks for this chunk
                auto nextLinebreak = currentLinebreak;
                while (nextLinebreak < job->linebreaks.size()) {
                    if (job->linebreaks[nextLinebreak++].first >= processed) {
                        break;
                    }
                }
                std::vector<Linebreak> newLinebreaks;
                newLinebreaks.reserve(nextLinebreak - currentLinebreak);
                std::copy(job->linebreaks.begin() + currentLinebreak,
                          job->linebreaks.begin() + nextLinebreak,
                          std::back_inserter(newLinebreaks));
                linebreaks = std::move(newLinebreaks);
                currentLinebreak = nextLinebreak;
            }

            if (linebreaks.empty()) {
                // nothing to do?
                continue;
            }

            // for each linebreak
            std::size_t prev = 0;
            for (const auto& lb : linebreaks) {
                auto line = std::u32string_view(chunk.data.begin() + prev, chunk.data.begin() + (lb.first - processedStart));

                hb_buffer_t* buf = hb_buffer_create();
                hb_buffer_add_utf32(buf, reinterpret_cast<const uint32_t*>(line.data()),
                                    line.size(), 0, line.size());
                hb_buffer_guess_segment_properties(buf);
                hb_shape(chunk.font, buf, nullptr, 0);
                buffers.push_back({ buf });
                prev = lb.first - processedStart;
            }

            // memmove the remaining chunk data if any
            const auto lbidx = linebreaks.back().first - processedStart;
            if (lbidx + 1 < chunk.data.size()) {
                ::memmove(chunk.data.data(), chunk.data.data() + lbidx + 1, chunk.data.size() - (lbidx + 1));
                chunk.data.resize(chunk.data.size() - (lbidx + 1));
            } else {
                chunk.data.clear();
                hb_font_destroy(chunk.font);
                chunk.font = nullptr;
            }
        }
    });
}
} // namespace spurv

using namespace spurv;

Layout::Layout()
{
}

Layout::~Layout()
{
    clearLines();
    clearFont();
}

void Layout::setFont(const Font& font)
{
    clearFont();
    mFontBlob = hb_blob_create_from_file(font.file().c_str());
    if (mFontBlob == nullptr) {
        return;
    }
    mFontFace = hb_face_create(mFontBlob, 0);
    if (mFontFace == nullptr) {
        hb_blob_destroy(mFontBlob);
        mFontBlob = nullptr;
    }
    mFontFont = hb_font_create(mFontFace);
    if (mFontFont == nullptr) {
        hb_face_destroy(mFontFace);
        mFontFace = nullptr;
        hb_blob_destroy(mFontBlob);
        mFontBlob = nullptr;
    }
}

void Layout::reset(Mode mode)
{
    clearLines();
    mMode = mode;
    mFinalized = false;
}

void Layout::calculate(std::u32string&& data, std::vector<Linebreak>&& linebreaks)
{
    bool createdJob = false;
    if (!mJob) {
        createdJob = true;
        mJob = std::make_shared<LayoutJob>();
    }
    std::lock_guard lock(mJob->mutex);
    if (!mJob->running) {
        if (!createdJob) {
            mJob = std::make_shared<LayoutJob>();
        }
        mJob->layout = this;
        LayoutJob::runJob(mJob);
    }
    const auto dataSize = data.size();
    mJob->chunks.push_back({ std::move(data), hb_font_reference(mFontFont) });
    if (linebreaks.empty()) {
        return;
    }
    if (mJob->linebreaks.empty()) {
        assert(mJob->length == 0);
        mJob->linebreaks = std::move(linebreaks);
    } else {
        std::transform(linebreaks.cbegin(), linebreaks.cend(), linebreaks.begin(), [adjust = mJob->length](const auto& b) {
            return std::make_pair(b.first + adjust, b.second);
        });
        auto& lbBack = mJob->linebreaks.back();
        const auto& lbFront = linebreaks.front();
        if (lbBack.first + 1 == lbFront.first && lbBack.second == 0x000D && lbFront.second == 0x000A) {
            // replace old back with new front and take the rest
            lbBack.first = lbFront.first;
            lbBack.second = 0x000A;
            if (linebreaks.size() > 1) {
                mJob->linebreaks.insert(mJob->linebreaks.end(),
                                        std::make_move_iterator(linebreaks.begin() + 1),
                                        std::make_move_iterator(linebreaks.end()));
            }
        } else {
            mJob->linebreaks.insert(mJob->linebreaks.end(),
                                    std::make_move_iterator(linebreaks.begin()),
                                    std::make_move_iterator(linebreaks.end()));
        }
    }
    mJob->length += dataSize;
}

void Layout::notifyLines()
{
    if (mMode == Mode::Single || mFinalized) {
        mOnReady.emit();
    }
}

void Layout::finalize()
{
    mFinalized = true;
    bool shouldEmit = false;
    {
        assert(mJob);
        std::lock_guard lock(mJob->mutex);
        if (!mJob->running) {
            shouldEmit = true;
        }
    }
    if (shouldEmit) {
        mOnReady.emit();
    }
}

void Layout::clearFont()
{
    if (mFontFont != nullptr) {
        hb_font_destroy(mFontFont);
        mFontFont = nullptr;
    }
    if (mFontFace != nullptr) {
        hb_face_destroy(mFontFace);
        mFontFace = nullptr;
    }
    if (mFontBlob != nullptr) {
        hb_blob_destroy(mFontBlob);
        mFontBlob = nullptr;
    }
}

void Layout::clearLines()
{
    for (auto& l : mLines) {
        if (l.buffer != nullptr) {
            hb_buffer_destroy(l.buffer);
        }
    }
    mLines.clear();
}
