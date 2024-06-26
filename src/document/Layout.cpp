#include "Layout.h"
#include <EventLoop.h>
#include <Logger.h>
#include <ThreadPool.h>
#include <uni_algo/ranges_word.h>
#include <mutex>
#include <cassert>
#include <cstring>

namespace spurv {
struct LayoutChunk
{
    std::u16string data;
    Font font;
};

class LayoutJob
{
public:
    std::mutex mutex;
    std::vector<LayoutChunk> chunks;
    std::vector<Linebreak> linebreaks;
    std::size_t length = 0, posted = 0;
    bool running = false;
    Layout* layout = nullptr;

    static void runJob(std::shared_ptr<LayoutJob> job, std::size_t prevProcessed, std::size_t startCluster);
};

void LayoutJob::runJob(std::shared_ptr<LayoutJob> job, std::size_t prevProcessed, std::size_t startCluster)
{
    job->running = true;
    auto loop = EventLoop::eventLoop();
    ThreadPool::mainThreadPool()->post([job = std::move(job), loop, prevProcessed, startCluster]() -> void {
        std::size_t processed = 0, processedStart = 0, total = 0;
        std::size_t currentChunk = 0;
        std::size_t currentLinebreak = 0;
        std::size_t prevEndCluster = startCluster;
        LayoutChunk chunk;
        std::vector<Linebreak> linebreaks;
        std::vector<Layout::LineInfo> buffers;

        for (;; ++currentChunk) {
            {
                std::lock_guard lock(job->mutex);
                if (!buffers.empty()) {
                    ++job->posted;
                    loop->post([buffers = std::move(buffers), layout = job->layout, total]() -> void {
                        const auto endCluster = buffers.empty() ? 0 : buffers.back().endCluster + 1;
                        layout->mLines.reserve(layout->mLines.size() + buffers.size());
                        layout->mLines.insert(layout->mLines.end(), buffers.begin(), buffers.end());
                        layout->notifyLines(total, endCluster);
                    });
                }
                if (currentChunk == job->chunks.size()) {
                    job->running = false;
                    return;
                }
                // take this chunk
                if (chunk.font.isValid() && chunk.font == job->chunks[currentChunk].font) {
                    // this is bad, we have left over data but this new chunk
                    // has a different font. so discard the previous data
                    chunk.font.clear();
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
                        --nextLinebreak;
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
            std::size_t prev = 0, accum = 0;
            for (const auto& lb : linebreaks) {
                const auto line = std::u16string_view(chunk.data.begin() + prev, chunk.data.begin() + (lb.first - processedStart));

                std::vector<std::size_t> words;
                auto u16words = una::views::word::utf16(line);
                auto u16wordit = u16words.begin();
                if (u16wordit != u16words.end()) {
                    words.push_back(u16wordit.begin() - line.begin());
                    while (u16wordit != u16words.end()) {
                        words.push_back(u16wordit.end() - line.begin());
                        ++u16wordit;
                    }
                }

                hb_buffer_t* buf = hb_buffer_create();
                // hb_buffer_set_cluster_level(buf, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS);
                hb_buffer_add_utf16(buf, reinterpret_cast<const uint16_t*>(line.data()),
                                    line.size(), 0, line.size());
                hb_buffer_guess_segment_properties(buf);
                hb_shape(chunk.font.font(), buf, nullptr, 0);
                uint32_t glyphCount;
                uint32_t highLineCluster = 0;
                auto glyphInfo = hb_buffer_get_glyph_infos(buf, &glyphCount);
                for (uint32_t gi = 0; gi < glyphCount; ++gi) {
                    if (glyphInfo[gi].cluster > highLineCluster) {
                        highLineCluster = glyphInfo[gi].cluster;
                    }
                }

                buffers.push_back({
                        buf,
                        processedStart + prevProcessed + accum,
                        processedStart + line.size() + prevProcessed + accum,
                        prevEndCluster,
                        prevEndCluster + static_cast<std::size_t>(highLineCluster + 1),
                        std::move(words),
                        chunk.font
                    });
                prevEndCluster += highLineCluster + 1;
                prev = lb.first - processedStart + 1;
                accum = lb.first - processedStart;
            }
            total = accum;

            // memmove the remaining chunk data if any
            const auto lbidx = linebreaks.back().first - processedStart;
            if (lbidx + 1 < chunk.data.size()) {
                ::memmove(chunk.data.data(), chunk.data.data() + lbidx + 1, chunk.data.size() - (lbidx + 1));
                chunk.data.resize(chunk.data.size() - (lbidx + 1));
            } else {
                chunk.data.clear();
                chunk.font.clear();
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
}

void Layout::setFont(const Font& font)
{
    mFont = font;
}

void Layout::reset(Mode mode)
{
    clearLines();
    mMode = mode;
    mFinalized = false;
    mReceived = 0;
    mProcessed = 0;
    mOnReady.disconnectAll();
}

void Layout::calculate(std::u16string&& data, std::vector<Linebreak>&& linebreaks)
{
    if (!mFont.isValid()) {
        spdlog::error("Layout font is not valid");
        return;
    }
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
        LayoutJob::runJob(mJob, mProcessed, mEndCluster);
    }
    const auto dataSize = data.size();
    mJob->chunks.push_back({ std::move(data), mFont });
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

void Layout::notifyLines(std::size_t processed, std::size_t endCluster)
{
    ++mReceived;
    mProcessed += processed;
    mEndCluster = endCluster;
    if (mMode == Mode::Single) {
        mOnReady.emit();
    } else if (mFinalized) {
        // refinalize
        finalize();
    }
}

void Layout::finalize()
{
    mFinalized = true;
    bool shouldEmit = false;
    if (mJob) {
        std::lock_guard lock(mJob->mutex);
        if (!mJob->running && mJob->posted == mReceived) {
            shouldEmit = true;
        }
    } else {
        shouldEmit = true;
    }
    if (shouldEmit) {
        mOnReady.emit();
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

std::pair<std::size_t, const Layout::LineInfo*> Layout::lineForCluster(std::size_t cluster) const
{
    const LineInfo findLine = {
        nullptr, 0, 0, cluster
    };
    auto it = std::upper_bound(mLines.begin(), mLines.end(), findLine, [](const LineInfo& i1, const LineInfo& i2) -> bool {
        return i1.startCluster < i2.startCluster;
    });
    if (it != mLines.begin()) {
        return std::make_pair(static_cast<std::size_t>(it - mLines.begin() - 1), &*(it - 1));
    }
    if (!mLines.empty()) {
        assert(cluster < mLines.front().startCluster);
        return std::make_pair(static_cast<std::size_t>(0), &mLines.front());
    }
    return std::make_pair(static_cast<std::size_t>(0), nullptr);
}
