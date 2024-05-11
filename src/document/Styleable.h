#pragma once

#include <Color.h>
#include <EventEmitter.h>
#include <Geometry.h>
#include <qssdocument.h>
#include <yoga/Yoga.h>
#include <optional>
#include <string>
#include <vector>

namespace spurv {

/* Styleables

   generally the selector syntax matches that of Qt style sheets,
   https://doc.qt.io/qt-6/stylesheet-syntax.html

   frame is a frame (frame)
   container is a frame (frame or container)
   view is a frame (frame or view)
   editor is a special class that only matches the top level container
   .class matches classes added with addClass(class)
   frame > frame matches any frames that are children of other frames
   editor > frame matches any direct frame children of the editor
   #<name> matches a named frame/view (frame#<name> or view#<name>)
   :active matches the active view (view:active)
   :hover matches if the mouse is hovering the frame or view (frame:hover or view:hover)
   :pressed matches if the mouse is pressed inside the frame or view (frame:pressed or view:pressed)
   ### maybe support the is/where/not/has pseudo-classes, i.e. frame:has(> view:active) or frame:not(:has(> view:active))

   additionally js can assign tags to a frame using setTag(key:string, value:string) and removeTag(key:string),
   these can be matched as follows frame[key="value"] or view[key="value"]
*/

class Styleable
{
public:
    Styleable();
    Styleable(Styleable&&);
    virtual ~Styleable();

    Styleable& operator=(Styleable&&);

    void makeRoot();

    void setSelector(const std::string& selector);
    void setSelector(const qss::Selector& selector);
    const qss::Selector& selector() const;

    bool matchesSelector(const std::string& selector) const;
    bool matchesSelector(const qss::Selector& selector) const;
    static bool matchesSelector(const Styleable* styleable, const qss::Selector& selector);
    static uint64_t selectorSpecificity(const qss::Selector& selector);

    enum class StylesheetMode : uint32_t {
        Replace,
        Merge
    };
    void setStylesheet(const std::string& qss, StylesheetMode mode = StylesheetMode::Replace);
    void setStylesheet(const qss::Document& qss, StylesheetMode mode = StylesheetMode::Replace);
    const qss::Document& stylesheet() const;

    void addStyleableChild(Styleable* child);
    void removeStyleableChild(Styleable* child);

    void setName(const std::string& name);
    std::string name() const;

    void setTag(const std::string& key, const std::string& value);
    void removeTag(const std::string& key);

    void addClass(const std::string& name);
    void removeClass(const std::string& name);

    enum class ColorType : uint32_t {
        Background,
        Foreground,
        Border,
        Shadow
    };
    const std::optional<Color>& color(ColorType type) const;
    // left, top, right, bottom
    const std::array<uint32_t, 4>& borderRadius() const;
    const std::array<uint32_t, 4>& border() const;
    const std::array<uint32_t, 4>& padding() const;
    const std::array<uint32_t, 4>& margin() const;

    struct BoxShadow {
        float hoffset = {};
        float voffset = {};
        float blur = {};
        float spread = {};
    };
    const BoxShadow& boxShadow() const;

    EventEmitter<void(const std::string&)>& onNameChanged();
    EventEmitter<void()>& onAppliedStylesheet();

protected:
    virtual void updateLayout(const Rect& rect) = 0;

    void mergeParentStylesheet(const qss::Document& qss);
    void unmergeParentStylesheet();

    qss::Selector& mutableSelector();

    void clearStyleData();
    void applyStylesheet();

    void relayout();

protected:
    qss::Selector mSelector;
    qss::Document mQss, mMergedQss;
    std::array<std::optional<Color>, 4> mColors;
    // left, top, right, bottom
    std::array<uint32_t, 4> mBorder = {}, mBorderRadius = {}, mPadding = {}, mMargin = {};
    BoxShadow mBoxShadow = {};
    YGNodeRef mYogaNode = nullptr;

    Styleable* mParent = nullptr;
    std::vector<Styleable*> mChildren;

    EventEmitter<void(const std::string&)> mOnNameChanged;
    EventEmitter<void()> mOnAppliedStylesheet;

private:
    static bool matchesSelector(const Styleable* styleable, const qss::Selector& selector, std::size_t inputOffset);

private:
    Styleable(const Styleable&) = delete;
    Styleable& operator=(const Styleable&) = delete;
};

inline EventEmitter<void(const std::string&)>& Styleable::onNameChanged()
{
    return mOnNameChanged;
}

inline EventEmitter<void()>& Styleable::onAppliedStylesheet()
{
    return mOnAppliedStylesheet;
}

inline void Styleable::setSelector(const qss::Selector& selector)
{
    mSelector = selector;
}

inline void Styleable::setSelector(const std::string& selector)
{
    setSelector(qss::Selector(selector));
}

inline const qss::Selector& Styleable::selector() const
{
    return mSelector;
}

inline qss::Selector& Styleable::mutableSelector()
{
    return mSelector;
}

inline bool Styleable::matchesSelector(const std::string& selector) const
{
    return matchesSelector(qss::Selector(selector));
}

inline const qss::Document& Styleable::stylesheet() const
{
    return mQss;
}

inline void Styleable::setName(const std::string& name)
{
    const std::string oldName = mSelector[0].id();
    mutableSelector()[0].id(name);
    mOnNameChanged.emit(oldName);
}

inline std::string Styleable::name() const
{
    return mSelector[0].id();
}

inline void Styleable::setTag(const std::string& key, const std::string& value)
{
    mSelector[0].on(key, value);
}

inline void Styleable::removeTag(const std::string& key)
{
    mSelector[0].off(key);
}

inline void Styleable::addClass(const std::string& name)
{
    mSelector[0].clazz(name);
}

inline void Styleable::removeClass(const std::string& name)
{
    mSelector[0].noclazz(name);
}

inline const std::optional<Color>& Styleable::color(ColorType type) const
{
    return mColors[static_cast<std::underlying_type_t<ColorType>>(type)];
}

inline const std::array<uint32_t, 4>& Styleable::borderRadius() const
{
    return mBorderRadius;
}

inline const std::array<uint32_t, 4>& Styleable::border() const
{
    return mBorder;
}

inline const std::array<uint32_t, 4>& Styleable::padding() const
{
    return mPadding;
}

inline const std::array<uint32_t, 4>& Styleable::margin() const
{
    return mMargin;
}

inline const Styleable::BoxShadow& Styleable::boxShadow() const
{
    return mBoxShadow;
}

} // namespace spurv
