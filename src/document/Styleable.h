#pragma once

#include <qssdocument.h>
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
   .frame matches frames but not views and also not the editor
   frame > frame matches any frames that are children of other frames
   editor > frame matches any direct frame children of the editor
   #<name> matches a named frame/view (frame#<name> or view#<name>)
   ::border is the styleable border of a frame (frame::border or view::border)
   .frame::border matches borders of frames but not of views
   :active matches the active view (view:active)
   :hover matches if the mouse is hovering the frame or view (frame:hover or view:hover)
   :pressed matches if the mouse is pressed inside the frame or view (frame:pressed or view:pressed)
   you can match parent frames of the active view by doing frame > view:active

   additionally js can assign tags to a frame using setTag(key:string, value:string) and removeTag(key:string),
   these can be matched as follows frame[key="value"] or view[key="value"]
*/

class Styleable
{
public:
    Styleable() = default;
    Styleable(Styleable&&) = default;
    Styleable(const Styleable&) = default;
    virtual ~Styleable() = default;

    Styleable& operator=(Styleable&&) = default;
    Styleable& operator=(const Styleable&) = default;

    void setSelector(const std::string& selector);
    void setSelector(const qss::Selector& selector);
    const qss::Selector& selector() const;

    bool matchesSelector(const std::string& selector) const;
    bool matchesSelector(const qss::Selector& selector) const;
    static bool matchesSelector(const qss::Selector& selector1, const qss::Selector& selector2);

    enum class StylesheetMode {
        Replace,
        Merge
    };
    void setStylesheet(const std::string& qss, StylesheetMode mode = StylesheetMode::Replace);
    void setStylesheet(const qss::Document& qss, StylesheetMode mode = StylesheetMode::Replace);
    const qss::Document& stylesheet() const;

    void addStyleableChild(Styleable* child);
    void removeStyleableChild(Styleable* child);

    virtual void setName(const std::string& name) = 0;
    std::string name() const;

    void addClass(const std::string& name);
    void removeClass(const std::string& name);

protected:
    void mergeParentStylesheet(const qss::Document& qss);
    void unmergeParentStylesheet();

    qss::Selector& mutableSelector();

protected:
    qss::Selector mSelector;
    qss::Document mQss, mMergedQss;

    Styleable* mParent = nullptr;
    std::vector<Styleable*> mChildren;
};

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

inline std::string Styleable::name() const
{
    return mSelector[0].id();
}

inline void Styleable::addClass(const std::string& name)
{
    mSelector[0].clazz(name);
}

inline void Styleable::removeClass(const std::string& name)
{
    mSelector[0].noclazz(name);
}

} // namespace spurv
