#include "Styleable.h"
#include "Logger.h"
#include <cassert>

using namespace spurv;

enum class StyleName {
    Unknown,
    Empty,
    Star,
    Editor,
    Frame,
    Container,
    View,
    Document
};

static inline StyleName nameToStyleName(const std::string& name)
{
    switch (name.size()) {
    case 0:
        return StyleName::Empty;
    case 1:
        if (name[0] == '*') {
            return StyleName::Star;
        }
        break;
    case 4: // view?
        if (strncasecmp(name.c_str(), "view", 4) == 0) {
            return StyleName::View;
        }
        break;
    case 5: // frame?
        if (strncasecmp(name.c_str(), "frame", 5) == 0) {
            return StyleName::Frame;
        }
        break;
    case 6: // editor?
        if (strncasecmp(name.c_str(), "editor", 6) == 0) {
            return StyleName::Editor;
        }
        break;
    case 8: // document?
        if (strncasecmp(name.c_str(), "document", 8) == 0) {
            return StyleName::Document;
        }
        break;
    case 9: // container?
        if (strncasecmp(name.c_str(), "container", 9) == 0) {
            return StyleName::Container;
        }
        break;
    default:
        break;
    }
    return StyleName::Unknown;
}

void Styleable::setStylesheet(const qss::Document& qss, StylesheetMode mode)
{
    if (mode == StylesheetMode::Replace) {
        mQss = qss;
    } else {
        mQss += qss;
    }
    mMergedQss = mQss;
    for (auto child : mChildren) {
        child->mergeParentStylesheet(mMergedQss);
    }
}

void Styleable::setStylesheet(const std::string& qss, StylesheetMode mode)
{
    setStylesheet(qss::Document(qss), mode);
}

void Styleable::mergeParentStylesheet(const qss::Document& qss)
{
    mMergedQss = qss + mQss;
    for (auto child : mChildren) {
        child->mergeParentStylesheet(mMergedQss);
    }
}

void Styleable::unmergeParentStylesheet()
{
    mMergedQss = mQss;
    for (auto child : mChildren) {
        child->mergeParentStylesheet(mMergedQss);
    }
}

void Styleable::addStyleableChild(Styleable* child)
{
    auto it = std::find(mChildren.begin(), mChildren.end(), child);
    if (it == mChildren.end()) {
        mChildren.push_back(child);
        child->mergeParentStylesheet(mQss);
        child->mParent = this;
    }
}

void Styleable::removeStyleableChild(Styleable* child)
{
    auto it = std::find(mChildren.begin(), mChildren.end(), child);
    if (it != mChildren.end()) {
        mChildren.erase(it);
        child->unmergeParentStylesheet();
        child->mParent = nullptr;
    }
}

bool Styleable::matchesSelector(const qss::Selector& inputSelector, const qss::Selector& styleSelector)
{
    auto inputCopy = inputSelector;
    // spdlog::info("matches {} {}", inputCopy.toString(), styleSelector.toString());
    const std::size_t count = styleSelector.fragmentCount();
    if (inputCopy.fragmentCount() > count) {
        return false;
    }
    if (count == 0) {
        return true;
    }

    auto matchSelectorElement = [](const qss::SelectorElement& inputElem, const qss::SelectorElement& styleElem) -> bool {
        const auto inputName = nameToStyleName(inputElem.name());
        const auto styleName = nameToStyleName(styleElem.name());
        if (styleName == StyleName::Unknown || styleName == StyleName::Star) {
            return false;
        }
        switch (inputName) {
        case StyleName::Editor:
            // editor only matches editor
            if (styleName != StyleName::Editor) {
                return false;
            }
            break;
        case StyleName::Frame:
            // frame matches frame, container, view and editor
            if (styleName != StyleName::Frame && styleName != StyleName::Container && styleName != StyleName::View && styleName != StyleName::Editor) {
                return false;
            }
            break;
        case StyleName::Container:
            // container matches container and editor
            if (styleName != StyleName::Container && styleName != StyleName::Editor) {
                return false;
            }
            break;
        case StyleName::View:
            // view only matches view
            if (styleName != StyleName::View) {
                return false;
            }
            break;
        case StyleName::Document:
            // document only matches document
            if (styleName != StyleName::Document) {
                return false;
            }
            break;
        case StyleName::Empty:
        case StyleName::Star:
            // empty or star matches everything
            break;
        case StyleName::Unknown:
            return false;
        }
        return true;
    };

    size_t inputAdd = 0;
    const std::size_t inputSub = count - inputCopy.fragmentCount();
    for (std::size_t idx = count; idx - inputSub + inputAdd > 0; --idx) {
        auto* inputElem = &inputCopy[idx - 1 - inputSub + inputAdd];
        auto* styleElem = &styleSelector[idx - 1];
        // spdlog::info("- elem match {} {} -- {} {}",
        //              inputElem->name(), static_cast<uint32_t>(inputElem->position()),
        //              styleElem->name(), static_cast<uint32_t>(styleElem->position()));
        assert(inputElem->position() == qss::SelectorElement::PARENT
               || inputElem->position() == qss::SelectorElement::CHILD
               || inputElem->position() == qss::SelectorElement::DESCENDANT);
        assert(styleElem->position() == qss::SelectorElement::PARENT
               || styleElem->position() == qss::SelectorElement::CHILD);
        if (!matchSelectorElement(*inputElem, *styleElem)) {
            return false;
        }
        // remove the name and sub control, name we've already matched and sub control needs to be check on the outside
        inputElem->name(std::string {}).sub(std::string {});
        if (!inputElem->isGeneralizedFrom(*styleElem)) {
            return false;
        }
        // if the input is a descendant then we need to find the next matching style element
        if (inputElem->position() == qss::SelectorElement::DESCENDANT) {
            if (idx == 1) {
                // we're at the top element, everything is good
                return true;
            }

            --idx;
            while (idx > 0) {
                inputElem = &inputCopy[idx - 1 - inputSub + inputAdd];
                styleElem = &styleSelector[idx - 1];
                if (matchSelectorElement(*inputElem, *styleElem)) {
                    qss::SelectorElement inputElemCopy = *inputElem;
                    inputElemCopy.name(std::string {}).sub(std::string {});
                    if (inputElemCopy.isGeneralizedFrom(*styleElem)) {
                        if (inputElemCopy.position() == qss::SelectorElement::DESCENDANT) {
                            // this is a bit inefficient but we need to do the desendantness again
                            ++idx;
                        }
                        break;
                    }
                }
                ++inputAdd;
                --idx;
            }
            if (idx == 0) {
                return false;
            }
        }
    }
    return true;
}

void Styleable::buildSelector(qss::Selector& selector, const Styleable* styleable)
{
    if (styleable->mParent) {
        buildSelector(selector, styleable->mParent);
    }
    const auto pos = selector.fragmentCount() > 0 ? qss::SelectorElement::CHILD : qss::SelectorElement::PARENT;
    assert(styleable->mSelector.fragmentCount() == 1);
    selector.append(styleable->mSelector[0], pos);
}

bool Styleable::matchesSelector(const qss::Selector& selector) const
{
    qss::Selector styleSelector;
    buildSelector(styleSelector, this);

    return matchesSelector(selector, styleSelector);
}
