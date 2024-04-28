#include "Styleable.h"

using namespace spurv;

void Styleable::setStylesheet(const qss::Document& qss, StylesheetMode mode)
{
    if (mode == StylesheetMode::Replace) {
        mQss = qss;
    } else {
        mQss += qss;
    }
    for (auto child : mChildren) {
        child->mergeParentStylesheet(mQss);
    }
}

void Styleable::setStylesheet(const std::string& qss, StylesheetMode mode)
{
    setStylesheet(qss::Document(qss), mode);
}

void Styleable::mergeParentStylesheet(const qss::Document& qss)
{
    mQss = qss + mQss;
    for (auto child : mChildren) {
        child->mergeParentStylesheet(mQss);
    }
}

void Styleable::addStyleableChild(Styleable* child)
{
    auto it = std::find(mChildren.begin(), mChildren.end(), child);
    if (it == mChildren.end()) {
        mChildren.push_back(child);
        child->mergeParentStylesheet(mQss);
    }
}

void Styleable::removeStyleableChild(Styleable* child)
{
    auto it = std::find(mChildren.begin(), mChildren.end(), child);
    if (it != mChildren.end()) {
        mChildren.erase(it);
    }
}
