#pragma once

#include "Easing.h"

#include <EventEmitter.h>
#include <EventLoop.h>
#include <Geometry.h>
#include <TextLine.h>
#include <TextProperty.h>

#include <volk.h>

#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace spurv {

struct RendererImpl;
struct GlyphsCreated;
class GenericPoolBase;
class GlyphAtlas;

struct RenderViewData
{
    Rect frame;
    Rect content;
    Color backgroundColor;
    Color viewColor;
    Color borderColor = {};
    Color shadowColor = {};
    std::array<uint32_t, 4> borderRadius = {};
    std::array<uint32_t, 2> shadowOffset = {};
    float edgeSoftness = {};
    float borderThickness = {};
    float borderSoftness = {};
    float shadowSoftness = {};
};

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

    EventEmitter<void()>& onReady();

    void setRenderViewData(const std::string& ident, const RenderViewData& data);
    void setRenderViewData(std::string&& ident, const RenderViewData& data);

    void addTextLines(const std::string& ident, std::vector<TextLine>&& lines);
    void addTextLines(std::string&& ident, std::vector<TextLine>&& lines);
    void clearTextLines(const std::string& ident);
    void clearTextLines(std::string&& ident);
    void addTextProperties(const std::string& ident, std::vector<TextProperty>&& lines);
    void addTextProperties(std::string&& ident, std::vector<TextProperty>&& lines);
    void clearTextProperties(const std::string& ident);
    void clearTextProperties(std::string&& ident);

    void setPropertyInt(const std::string& ident, Property prop, int32_t value);
    void setPropertyInt(std::string&& ident, Property prop, int32_t value);
    void setPropertyFloat(const std::string& ident, Property prop, float value);
    void setPropertyFloat(std::string&& ident, Property prop, float value);
    void animatePropertyInt(const std::string& ident, Property prop, int32_t value, uint64_t ms, Ease ease);
    void animatePropertyInt(std::string&& ident, Property prop, int32_t value, uint64_t ms, Ease ease);
    void animatePropertyFloat(const std::string& ident, Property prop, float value, uint64_t ms, Ease ease);
    void animatePropertyFloat(std::string&& ident, Property prop, float value, uint64_t ms, Ease ease);

    void renameIdentifier(const std::string& oldIdent, const std::string& newIdent);

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
