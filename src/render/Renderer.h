#pragma once

#include <EventEmitter.h>
#include <EventLoop.h>
#include <TextLine.h>
#include <TextProperty.h>
#include "Box.h"
#include "Easing.h"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <memory>
#include <optional>
#include <cstdint>
#include <cassert>
#include <volk.h>

namespace spurv {

struct RendererImpl;
struct GlyphsCreated;
class GenericPoolBase;
class GlyphAtlas;

class Renderer
{
public:
    ~Renderer();

    enum class Property : uint32_t
    {
        FirstLine,
        Max
    };

    static void initialize(const std::filesystem::path &appPath);
    static void destroy();
    static Renderer* instance();
    static EventLoop* eventLoop();

    using BoxRow = std::vector<Box>;
    using Boxes = std::vector<BoxRow>;

    void setBoxes(Boxes&& boxes);

    EventEmitter<void()>& onReady();

    void addTextLines(uint32_t box, std::vector<TextLine>&& lines);
    void clearTextLines(uint32_t box);
    void addTextProperties(uint32_t box, std::vector<TextProperty>&& lines);
    void clearTextProperties(uint32_t box);

    void setPropertyInt(uint32_t box, Property prop, int32_t value);
    void setPropertyFloat(uint32_t box, Property prop, float value);
    void animatePropertyInt(uint32_t box, Property prop, int32_t value, uint64_t ms, Ease ease);
    void animatePropertyFloat(uint32_t box, Property prop, float value, uint64_t ms, Ease ease);

    void afterCurrentFrame(std::function<void()>&& func);
    void afterTransfer(uint64_t value, std::function<void()>&& func);

private:
    Renderer(const std::filesystem::path &appPath);

    Renderer(Renderer&&) = delete;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(Renderer&&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void thread_internal();
    void render();
    void stop();

    bool recreateSwapchain();
    void glyphsCreated(GlyphsCreated&& created);

private:
    static std::unique_ptr<Renderer> sInstance;

    std::mutex mMutex;
    std::condition_variable mCond;
    std::thread mThread;
    std::unique_ptr<EventLoop> mEventLoop;
    EventEmitter<void()> mOnReady;

    RendererImpl* mImpl;
    bool mInitialized = false; //, mStopped = true;

    friend struct RendererImpl;
    friend class GlyphAtlas;
    friend class GenericPoolBase;
};

inline Renderer* Renderer::instance()
{
    assert(sInstance);
    return sInstance.get();
}

inline EventLoop* Renderer::eventLoop()
{
    assert(sInstance);
    assert(sInstance->mEventLoop);
    return sInstance->mEventLoop.get();
}

inline void Renderer::destroy()
{
    auto instance = sInstance.get();
    if (!instance)
        return;
    instance->stop();
    sInstance.reset();
}

inline EventEmitter<void()>& Renderer::onReady()
{
    return mOnReady;
}

} // namespace spurv
