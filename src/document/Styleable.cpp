#include "Styleable.h"
#include "Logger.h"
#include <UnorderedDense.h>
#include <cassert>

using namespace spurv;

struct StringSpaceSkipper
{
    StringSpaceSkipper(const std::string& s);

    std::string str;
    const char* cur;
    const char* next;
    const char* end;

    bool advance();

private:
    void advanceNext();
};

StringSpaceSkipper::StringSpaceSkipper(const std::string& s)
{
    str.resize(s.size());
    std::transform(s.begin(), s.end(), str.begin(), [](auto ch) {
        if (ch == ' ') {
            return '\0';
        }
        return ch;
    });
    cur = str.c_str();
    next = cur;
    end = str.c_str() + str.size();

    advanceNext();
}

void StringSpaceSkipper::advanceNext()
{
    while (next != end) {
        ++next;
        if (*next == '\0') {
            break;
        }
    }
}

bool StringSpaceSkipper::advance()
{
    while (next != end && *next == '\0') {
        ++next;
    }
    cur = next;
    if (cur != end) {
        advanceNext();
        assert(next > cur);
        return true;
    }
    return false;
}

enum class StyleSelectorName {
    Unknown,
    Empty,
    Star,
    Editor,
    Frame,
    Container,
    View,
    Document
};

static inline StyleSelectorName nameToStyleSelectorName(const std::string& name)
{
    switch (name.size()) {
    case 0:
        return StyleSelectorName::Empty;
    case 1:
        if (name[0] == '*') {
            return StyleSelectorName::Star;
        }
        break;
    case 4: // view?
        if (strncasecmp(name.c_str(), "view", 4) == 0) {
            return StyleSelectorName::View;
        }
        break;
    case 5: // frame?
        if (strncasecmp(name.c_str(), "frame", 5) == 0) {
            return StyleSelectorName::Frame;
        }
        break;
    case 6: // editor?
        if (strncasecmp(name.c_str(), "editor", 6) == 0) {
            return StyleSelectorName::Editor;
        }
        break;
    case 8: // document?
        if (strncasecmp(name.c_str(), "document", 8) == 0) {
            return StyleSelectorName::Document;
        }
        break;
    case 9: // container?
        if (strncasecmp(name.c_str(), "container", 9) == 0) {
            return StyleSelectorName::Container;
        }
        break;
    default:
        break;
    }
    return StyleSelectorName::Unknown;
}

enum class StyleRuleName {
    Unknown,
    BackgroundColor,
    Border,
    BorderWidth,
    // BorderStyle,
    BorderColor,
    BorderRadius,
    BoxShadow,
    Color,
    Flex,
    FlexBasis,
    FlexDirection,
    FlexFlow,
    FlexGrow,
    FlexShrink,
    FlexWrap,
    Gap,
    Margin,
    Padding,
    ShadowColor
};

static inline StyleRuleName nameToStyleRuleName(const std::string& name)
{
    switch (name.size()) {
    case 3:
        if (strncasecmp(name.c_str(), "gap", 4) == 0) {
            return StyleRuleName::Gap;
        }
        break;
    case 4:
        if (strncasecmp(name.c_str(), "flex", 4) == 0) {
            return StyleRuleName::Flex;
        }
        break;
    case 5:
        if (strncasecmp(name.c_str(), "color", 5) == 0) {
            return StyleRuleName::Color;
        }
        break;
    case 6:
        if (strncasecmp(name.c_str(), "border", 6) == 0) {
            return StyleRuleName::Border;
        }
        if (strncasecmp(name.c_str(), "margin", 6) == 0) {
            return StyleRuleName::Margin;
        }
        break;
    case 7:
        if (strncasecmp(name.c_str(), "padding", 7) == 0) {
            return StyleRuleName::Padding;
        }
        break;
    case 9:
        if (strncasecmp(name.c_str(), "flex-flow", 9) == 0) {
            return StyleRuleName::FlexFlow;
        }
        if (strncasecmp(name.c_str(), "flex-grow", 9) == 0) {
            return StyleRuleName::FlexGrow;
        }
        if (strncasecmp(name.c_str(), "flex-wrap", 9) == 0) {
            return StyleRuleName::FlexWrap;
        }
        break;
    case 10:
        if (strncasecmp(name.c_str(), "box-shadow", 10) == 0) {
            return StyleRuleName::BoxShadow;
        }
        if (strncasecmp(name.c_str(), "flex-basis", 10) == 0) {
            return StyleRuleName::FlexBasis;
        }
        break;
    case 11:
        if (strncasecmp(name.c_str(), "flex-shrink", 11) == 0) {
            return StyleRuleName::FlexShrink;
        }
        break;
    case 12:
        if (strncasecmp(name.c_str(), "border-width", 12) == 0) {
            return StyleRuleName::BorderWidth;
        }
        // if (strncasecmp(name.c_str(), "border-style", 12) == 0) {
        //     return StyleRuleName::BorderStyle;
        // }
        if (strncasecmp(name.c_str(), "border-color", 12) == 0) {
            return StyleRuleName::BorderColor;
        }
        if (strncasecmp(name.c_str(), "shadow-color", 12) == 0) {
            return StyleRuleName::ShadowColor;
        }
        break;
    case 13:
        if (strncasecmp(name.c_str(), "border-radius", 13) == 0) {
            return StyleRuleName::BorderRadius;
        }
        break;
    case 14:
        if (strncasecmp(name.c_str(), "flex-direction", 14) == 0) {
            return StyleRuleName::FlexDirection;
        }
        break;
    case 16:
        if (strncasecmp(name.c_str(), "background-color", 16) == 0) {
            return StyleRuleName::BackgroundColor;
        }
        break;
    default:
        break;
    }
    return StyleRuleName::Unknown;
}

struct StyleNumber
{
    float number;
    bool percentage;
};

static inline std::optional<StyleNumber> nameToStyleNumber(const char* name)
{
    char* endptr;
    auto num = strtof(name, &endptr);
    if (endptr > name) {
        if (*endptr == '\0') {
            return StyleNumber {
                num, false
            };
        } else if (*endptr == '%' && *(endptr + 1) == '\0') {
            return StyleNumber {
                num, true
            };
        }
    }
    return {};
}

enum class StyleFlexBasisName {
    Auto,
    Initial,
    Inherit
};

static inline std::variant<StyleNumber, StyleFlexBasisName, std::nullopt_t> nameToStyleFlexBasisName(const char* name, std::size_t size)
{
    switch (size) {
    case 4:
        if (strncasecmp(name, "auto", 4) == 0) {
            return StyleFlexBasisName::Auto;
        }
        break;
    case 7:
        if (strncasecmp(name, "initial", 7) == 0) {
            return StyleFlexBasisName::Initial;
        }
        if (strncasecmp(name, "inherit", 7) == 0) {
            return StyleFlexBasisName::Inherit;
        }
        break;
    default:
        break;
    }
    auto maybeStyleNumber = nameToStyleNumber(name);
    if (maybeStyleNumber.has_value()) {
        return maybeStyleNumber.value();
    }
    return std::nullopt;
}

enum class StyleFlexDirectionName {
    Unknown,
    Row,
    RowReverse,
    Column,
    ColumnReverse,
    Initial,
    Inherit
};

static inline StyleFlexDirectionName nameToStyleFlexDirectionName(const char* name, std::size_t size)
{
    switch (size) {
    case 3:
        if (strncasecmp(name, "row", 3) == 0) {
            return StyleFlexDirectionName::Row;
        }
        break;
    case 6:
        if (strncasecmp(name, "column", 6) == 0) {
            return StyleFlexDirectionName::Column;
        }
        break;
    case 7:
        if (strncasecmp(name, "initial", 7) == 0) {
            return StyleFlexDirectionName::Initial;
        }
        if (strncasecmp(name, "inherit", 7) == 0) {
            return StyleFlexDirectionName::Inherit;
        }
        break;
    case 11:
        if (strncasecmp(name, "row-reverse", 11) == 0) {
            return StyleFlexDirectionName::RowReverse;
        }
        break;
    case 14:
        if (strncasecmp(name, "column-reverse", 14) == 0) {
            return StyleFlexDirectionName::ColumnReverse;
        }
        break;
    default:
        break;
    }
    return StyleFlexDirectionName::Unknown;
}

enum class StyleFlexWrapName {
    Unknown,
    Wrap,
    WrapReverse,
    Initial,
    Inherit
};

static inline StyleFlexWrapName nameToStyleFlexWrapName(const char* name, std::size_t size)
{
    switch (size) {
    case 4:
        if (strncasecmp(name, "wrap", 4) == 0) {
            return StyleFlexWrapName::Wrap;
        }
        break;
    case 7:
        if (strncasecmp(name, "initial", 7) == 0) {
            return StyleFlexWrapName::Initial;
        }
        if (strncasecmp(name, "inherit", 7) == 0) {
            return StyleFlexWrapName::Inherit;
        }
        break;
    case 12:
        if (strncasecmp(name, "wrap-reverse", 12) == 0) {
            return StyleFlexWrapName::WrapReverse;
        }
        break;
    default:
        break;
    }
    return StyleFlexWrapName::Unknown;
}

enum class StyleFlexInitialInheritName {
    Initial,
    Inherit
};

static inline std::variant<StyleFlexInitialInheritName, StyleNumber, std::nullopt_t> nameToStyleFlexInitialInheritNumberName(const char* name, std::size_t size)
{
    switch (size) {
    case 7:
        if (strncasecmp(name, "initial", 7) == 0) {
            return StyleFlexInitialInheritName::Initial;
        }
        if (strncasecmp(name, "inherit", 7) == 0) {
            return StyleFlexInitialInheritName::Inherit;
        }
        break;
    default:
        break;
    }
    auto maybeStyleNumber = nameToStyleNumber(name);
    if (maybeStyleNumber.has_value()) {
        return maybeStyleNumber.value();
    }
    return std::nullopt;
}

Styleable::Styleable()
    : mYogaNode(YGNodeNew())
{
}

Styleable::Styleable(Styleable&& other)
    : mYogaNode(other.mYogaNode)
{
    other.mYogaNode = nullptr;
}

Styleable::~Styleable()
{
    if (mYogaNode) {
        YGNodeFree(mYogaNode);
    }
}

Styleable& Styleable::operator=(Styleable&& other)
{
    if (mYogaNode) {
        YGNodeFree(mYogaNode);
    }
    mYogaNode = other.mYogaNode;
    other.mYogaNode = nullptr;
    return *this;
}

void Styleable::makeRoot()
{
    // do I need to free this config later?
    auto cfg = YGConfigNew();
    YGConfigSetUseWebDefaults(cfg, true);
    YGNodeFree(mYogaNode);
    mYogaNode = YGNodeNewWithConfig(cfg);
}

void Styleable::setStylesheet(const qss::Document& qss, StylesheetMode mode)
{
    if (mode == StylesheetMode::Replace) {
        mQss = qss;
    } else {
        mQss += qss;
    }
    mMergedQss = mQss;
    applyStylesheet();
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
    applyStylesheet();
    for (auto child : mChildren) {
        child->mergeParentStylesheet(mMergedQss);
    }
}

void Styleable::unmergeParentStylesheet()
{
    mMergedQss = mQss;
    applyStylesheet();
    for (auto child : mChildren) {
        child->mergeParentStylesheet(mMergedQss);
    }
}

void Styleable::addStyleableChild(Styleable* child)
{
    auto it = std::find(mChildren.begin(), mChildren.end(), child);
    if (it == mChildren.end()) {
        YGNodeInsertChild(mYogaNode, child->mYogaNode, YGNodeGetChildCount(mYogaNode));
        mChildren.push_back(child);
        child->mParent = this;
        child->mergeParentStylesheet(mQss);
    }
}

void Styleable::removeStyleableChild(Styleable* child)
{
    auto it = std::find(mChildren.begin(), mChildren.end(), child);
    if (it != mChildren.end()) {
        mChildren.erase(it);
        child->unmergeParentStylesheet();
        child->mParent = nullptr;
        YGNodeRemoveChild(mYogaNode, child->mYogaNode);
    }
}

bool Styleable::matchesSelector(const Styleable* styleable, const qss::Selector& inputSelector, std::size_t inputOffset)
{
    if (inputOffset >= inputSelector.fragmentCount()) {
        return false;
    }

    const Styleable* topStyleable = styleable;
    qss::Selector inputCopy;
    inputCopy.operator=(inputSelector);
    // spdlog::info("matches {} {}", inputCopy.toString(), styleSelector.toString());

    auto matchSelectorElement = [](const qss::SelectorElement& styleElem, const qss::SelectorElement& inputElem) -> bool {
        const auto inputName = nameToStyleSelectorName(inputElem.name());
        const auto styleName = nameToStyleSelectorName(styleElem.name());
        if (styleName == StyleSelectorName::Unknown || styleName == StyleSelectorName::Star) {
            return false;
        }
        switch (inputName) {
        case StyleSelectorName::Editor:
            // editor only matches editor
            if (styleName != StyleSelectorName::Editor) {
                return false;
            }
            break;
        case StyleSelectorName::Frame:
            // frame matches frame, container, view and editor
            if (styleName != StyleSelectorName::Frame && styleName != StyleSelectorName::Container && styleName != StyleSelectorName::View && styleName != StyleSelectorName::Editor) {
                return false;
            }
            break;
        case StyleSelectorName::Container:
            // container matches container and editor
            if (styleName != StyleSelectorName::Container && styleName != StyleSelectorName::Editor) {
                return false;
            }
            break;
        case StyleSelectorName::View:
            // view only matches view
            if (styleName != StyleSelectorName::View) {
                return false;
            }
            break;
        case StyleSelectorName::Document:
            // document only matches document
            if (styleName != StyleSelectorName::Document) {
                return false;
            }
            break;
        case StyleSelectorName::Empty:
        case StyleSelectorName::Star:
            // empty or star matches everything
            break;
        case StyleSelectorName::Unknown:
            return false;
        }
        return true;
    };

    auto gotoAdjacent = [&inputCopy](std::size_t off) -> std::size_t {
        while (off > 2) {
            if (inputCopy[off - 1].position() == qss::SelectorElement::ADJACENT) {
                return off;
            }
            --off;
        }
        return std::numeric_limits<std::size_t>::max();
    };

    const auto inputCount = inputCopy.fragmentCount();
    const auto count = inputCount - inputOffset;
    for (std::size_t idx = count; idx > 0; --idx) {
        auto* inputElem = &inputCopy[idx - 1];
        const auto* styleElem = &styleable->mSelector[0];
        assert(styleElem->position() == qss::SelectorElement::PARENT
               || styleElem->position() == qss::SelectorElement::CHILD);

        if (!matchSelectorElement(*styleElem, *inputElem)) {
            idx = gotoAdjacent(idx);
            if (idx != std::numeric_limits<std::size_t>::max()) {
                styleable = topStyleable;
                continue;
            }
            return false;
        }
        // remove the name and sub control, name we've already matched and sub control needs to be checked on the outside
        inputElem->name(std::string {});
        if (idx == count) {
            inputElem->sub(std::string {});
        }
        if (!inputElem->isGeneralizedFrom(*styleElem)) {
            idx = gotoAdjacent(idx);
            if (idx != std::numeric_limits<std::size_t>::max()) {
                styleable = topStyleable;
                continue;
            }
            return false;
        }

        switch (inputElem->position()) {
        case qss::SelectorElement::PARENT:
        case qss::SelectorElement::ADJACENT:
            // top level, done
            return true;
        case qss::SelectorElement::CHILD:
            // direct child
            styleable = styleable->mParent;
            if (styleable == nullptr) {
                // no match
                idx = gotoAdjacent(idx);
                if (idx != std::numeric_limits<std::size_t>::max()) {
                    styleable = topStyleable;
                    continue;
                }
                return false;
            }
            break;
        case qss::SelectorElement::DESCENDANT: {
            // general child, might match any of the parents
            auto parent = styleable->mParent;
            while (parent != nullptr) {
                if (matchesSelector(parent, inputSelector, inputCount - idx + 1)) {
                    return true;
                }

                parent = parent->mParent;
            }
            idx = gotoAdjacent(idx);
            if (idx != std::numeric_limits<std::size_t>::max()) {
                styleable = topStyleable;
                continue;
            }
            return false; }
        case qss::SelectorElement::SIBLING: {
            // direct sibling
            if (styleable->mParent == nullptr) {
                // no match
                idx = gotoAdjacent(idx);
                if (idx != std::numeric_limits<std::size_t>::max()) {
                    styleable = topStyleable;
                    continue;
                }
                return false;
            }
            auto cit = std::find_if(styleable->mParent->mChildren.begin(), styleable->mParent->mChildren.end(),
                                    [styleable](Styleable* child) {
                                        return child == styleable;
                                    });
            if (cit == styleable->mParent->mChildren.end() || cit == styleable->mParent->mChildren.begin()) {
                idx = gotoAdjacent(idx);
                if (idx != std::numeric_limits<std::size_t>::max()) {
                    styleable = topStyleable;
                    continue;
                }
                return false;
            }
            --cit;
            styleable = *cit;
            break; }
        case qss::SelectorElement::GENERAL_SIBLING: {
            // general sibling
            if (styleable->mParent == nullptr) {
                // no match
                idx = gotoAdjacent(idx);
                if (idx != std::numeric_limits<std::size_t>::max()) {
                    styleable = topStyleable;
                    continue;
                }
                return false;
            }
            auto cit = std::find_if(styleable->mParent->mChildren.begin(), styleable->mParent->mChildren.end(),
                                    [styleable](Styleable* child) {
                                        return child == styleable;
                                    });
            if (cit == styleable->mParent->mChildren.end() || cit == styleable->mParent->mChildren.begin()) {
                idx = gotoAdjacent(idx);
                if (idx != std::numeric_limits<std::size_t>::max()) {
                    styleable = topStyleable;
                    continue;
                }
                return false;
            }
            --cit;
            while (cit != styleable->mParent->mChildren.begin()) {
                if (matchesSelector(*cit, inputSelector, inputCount - idx + 1)) {
                    return true;
                }
                --cit;
            }
            idx = gotoAdjacent(idx);
            if (idx != std::numeric_limits<std::size_t>::max()) {
                styleable = topStyleable;
                continue;
            }
            return false; }
        }
    }

    return true;
}

bool Styleable::matchesSelector(const Styleable* styleable, const qss::Selector& inputSelector)
{
    return matchesSelector(styleable, inputSelector, 0);
}

bool Styleable::matchesSelector(const qss::Selector& selector) const
{
    return matchesSelector(this, selector);
}

bool Styleable::isGeneralizedFrom(const qss::Selector& from, const qss::Selector& to)
{
    if (from.fragmentCount() == 0 || to.fragmentCount() == 0 || to.fragmentCount() < from.fragmentCount()) {
        return false;
    }
    auto fidx = from.fragmentCount();
    auto tidx = to.fragmentCount();
    while (fidx > 0) {
        if (!from[fidx - 1].isGeneralizedFrom(to[tidx - 1])) {
            return false;
        }
        --fidx;
        --tidx;
    }
    return true;
}

uint64_t Styleable::selectorSpecificity(const qss::Selector& selector)
{
    uint16_t ids = 0, attrs = 0, elems = 0;
    auto it = selector.cbegin();
    const auto end = selector.cend();
    while (it != end) {
        if (it->name() == "*") {
            continue;
        }
        if (!it->id().empty()) {
            ++ids;
        }
        if (!it->subControl().empty()) {
            ++attrs;
        }
        attrs += it->classes().size() + it->params().size();
        elems += it->pseudoStates().size() + 1; // + 1 for the element itself

        ++it;
    }
    return (static_cast<uint64_t>(ids) << 32) | (static_cast<uint64_t>(attrs) << 16) | static_cast<uint64_t>(elems);
}

template<std::size_t Size>
static inline std::size_t spacedNumbers(std::array<StyleNumber, Size>& array, const std::string& value)
{
    std::size_t numNumbers = 0;
    StringSpaceSkipper skipper(value);
    while (skipper.cur != skipper.end) {
        if (numNumbers == Size) {
            break;
        }
        auto num = nameToStyleNumber(skipper.cur);
        if (num.has_value()) {
            array[numNumbers++] = num.value();
        } else {
            return 0;
        }
        skipper.advance();
    }
    return numNumbers;
}

void Styleable::clearStyleData()
{
    mColors = {};
    mBorder = {};
    mBorderRadius = {};
    mBoxShadow = {};
}

void Styleable::applyStylesheet()
{
    clearStyleData();

    unordered_dense::map<std::string, std::pair<uint64_t, std::string>> matching;

    auto setColor = [this](ColorType type, std::optional<Color>&& color) -> void {
        mColors[static_cast<std::underlying_type_t<ColorType>>(type)] = std::move(color);
    };

    const auto dend = mMergedQss.cend();
    for (auto dit = mMergedQss.cbegin(); dit != dend; ++dit) {
        const auto& fragment = dit->first;
        const auto& selector = fragment.selector();
        if (matchesSelector(selector)) {
            const auto specificity = selectorSpecificity(selector);
            const auto& block = fragment.block();
            for (auto bit = block.cbegin(); bit != block.cend(); ++bit) {
                const auto& name = bit->first;
                auto mit = matching.find(name);
                if (mit == matching.end() || specificity >= mit->second.first) {
                    matching[name] = std::make_pair(specificity, bit->second.first);
                }
            }
        }
    }

    const auto mend = matching.cend();
    for (auto mit = matching.cbegin(); mit != mend; ++mit) {
        spdlog::info("looking at rule '{}'", mit->first);
        const auto ruleName = nameToStyleRuleName(mit->first);
        const auto& ruleValue = mit->second.second;
        switch (ruleName) {
        case StyleRuleName::BackgroundColor: {
            if (ruleValue.size() == 7 && strncasecmp(ruleValue.c_str(), "initial", 7) == 0) {
                setColor(ColorType::Background, {});
            } else {
                setColor(ColorType::Background, parseColor(ruleValue));
            }
            break; }
        case StyleRuleName::Border: {
            StringSpaceSkipper skipper(ruleValue);
            while (skipper.cur != skipper.end) {
                if (skipper.next - skipper.cur == 7 && strncasecmp(skipper.cur, "initial", 7) == 0) {
                    setColor(ColorType::Border, {});
                    mBorder = {};
                    YGNodeStyleSetBorder(mYogaNode, YGEdgeAll, 0.f);
                } else {
                    auto maybeNumber = nameToStyleNumber(skipper.cur);
                    if (maybeNumber.has_value()) {
                        if (!maybeNumber->percentage) {
                            mBorder.fill(static_cast<uint32_t>(maybeNumber->number));
                            YGNodeStyleSetBorder(mYogaNode, YGEdgeAll, maybeNumber->number);
                        }
                    } else {
                        setColor(ColorType::Border, parseColor(std::string(skipper.cur, skipper.next - skipper.cur)));
                    }
                }
                skipper.advance();
            }
            break; }
        case StyleRuleName::BorderWidth: {
            std::array<float, 4> borders;
            std::size_t numBorders = 0;
            StringSpaceSkipper skipper(ruleValue);
            while (skipper.cur != skipper.end) {
                if (numBorders == 4) {
                    break;
                }
                auto grow = nameToStyleFlexInitialInheritNumberName(skipper.cur, skipper.next - skipper.cur);
                if (std::holds_alternative<StyleNumber>(grow)) {
                    const auto& number = std::get<StyleNumber>(grow);
                    if (!number.percentage) {
                        borders[numBorders++] = number.number;
                    }
                } else if (std::holds_alternative<StyleFlexInitialInheritName>(grow)) {
                    if (std::get<StyleFlexInitialInheritName>(grow) == StyleFlexInitialInheritName::Initial) {
                        mBorder = {};
                        YGNodeStyleSetBorder(mYogaNode, YGEdgeAll, 0.f);
                        numBorders = 0;
                        break;
                    } else {
                        // ### support inherit?
                    }
                }
                skipper.advance();
            }
            switch (numBorders) {
            case 1:
                mBorder.fill(static_cast<uint32_t>(borders[0]));
                YGNodeStyleSetBorder(mYogaNode, YGEdgeAll, borders[0]);
                break;
            case 2:
                mBorder[0] = static_cast<uint32_t>(borders[1]);
                mBorder[1] = static_cast<uint32_t>(borders[0]);
                mBorder[2] = static_cast<uint32_t>(borders[1]);
                mBorder[3] = static_cast<uint32_t>(borders[0]);
                YGNodeStyleSetBorder(mYogaNode, YGEdgeTop, borders[0]);
                YGNodeStyleSetBorder(mYogaNode, YGEdgeBottom, borders[0]);
                YGNodeStyleSetBorder(mYogaNode, YGEdgeLeft, borders[1]);
                YGNodeStyleSetBorder(mYogaNode, YGEdgeRight, borders[1]);
                break;
            case 3:
                mBorder[0] = static_cast<uint32_t>(borders[1]);
                mBorder[1] = static_cast<uint32_t>(borders[0]);
                mBorder[2] = static_cast<uint32_t>(borders[1]);
                mBorder[3] = static_cast<uint32_t>(borders[2]);
                YGNodeStyleSetBorder(mYogaNode, YGEdgeTop, borders[0]);
                YGNodeStyleSetBorder(mYogaNode, YGEdgeLeft, borders[1]);
                YGNodeStyleSetBorder(mYogaNode, YGEdgeRight, borders[1]);
                YGNodeStyleSetBorder(mYogaNode, YGEdgeBottom, borders[2]);
                break;
            case 4:
                mBorder[0] = static_cast<uint32_t>(borders[3]);
                mBorder[1] = static_cast<uint32_t>(borders[0]);
                mBorder[2] = static_cast<uint32_t>(borders[1]);
                mBorder[3] = static_cast<uint32_t>(borders[2]);
                YGNodeStyleSetBorder(mYogaNode, YGEdgeTop, borders[0]);
                YGNodeStyleSetBorder(mYogaNode, YGEdgeRight, borders[1]);
                YGNodeStyleSetBorder(mYogaNode, YGEdgeBottom, borders[2]);
                YGNodeStyleSetBorder(mYogaNode, YGEdgeLeft, borders[3]);
                break;
            }
            break; }
        case StyleRuleName::BorderColor: {
            if (ruleValue.size() == 7 && strncasecmp(ruleValue.c_str(), "initial", 7) == 0) {
                setColor(ColorType::Border, {});
            } else {
                setColor(ColorType::Border, parseColor(ruleValue));
            }
            break; }
        case StyleRuleName::BorderRadius: {
            if (ruleValue.size() == 7 && strncasecmp(ruleValue.c_str(), "initial", 7) == 0) {
                mBorderRadius = {};
            } else {
                std::array<StyleNumber, 4> numbers;
                const auto numNumbers = spacedNumbers(numbers, ruleValue);
                switch (numNumbers) {
                case 1:
                    if (!numbers[0].percentage) {
                        mBorderRadius[0] = mBorderRadius[1] = mBorderRadius[2] = mBorderRadius[3] = static_cast<uint32_t>(numbers[0].number);
                    }
                    break;
                case 2:
                    if (!numbers[0].percentage) {
                        mBorderRadius[1] = mBorderRadius[3] = static_cast<uint32_t>(numbers[0].number);
                    }
                    if (!numbers[1].percentage) {
                        mBorderRadius[0] = mBorderRadius[2] = static_cast<uint32_t>(numbers[1].number);
                    }
                    break;
                case 3:
                    if (!numbers[0].percentage) {
                        mBorderRadius[1] = static_cast<uint32_t>(numbers[0].number);
                    }
                    if (!numbers[1].percentage) {
                        mBorderRadius[0] = mBorderRadius[2] = static_cast<uint32_t>(numbers[1].number);
                    }
                    if (!numbers[2].percentage) {
                        mBorderRadius[3] = static_cast<uint32_t>(numbers[2].number);
                    }
                    break;
                case 4:
                    if (!numbers[0].percentage) {
                        mBorderRadius[1] = static_cast<uint32_t>(numbers[0].number);
                    }
                    if (!numbers[1].percentage) {
                        mBorderRadius[2] = static_cast<uint32_t>(numbers[1].number);
                    }
                    if (!numbers[2].percentage) {
                        mBorderRadius[3] = static_cast<uint32_t>(numbers[2].number);
                    }
                    if (!numbers[3].percentage) {
                        mBorderRadius[0] = static_cast<uint32_t>(numbers[3].number);
                    }
                    break;
                }
            }
            break; }
        case StyleRuleName::BoxShadow: {
            if (ruleValue.size() == 7 && strncasecmp(ruleValue.c_str(), "initial", 7) == 0) {
                setColor(ColorType::Shadow, {});
                mBoxShadow = {};
            } else {
                setColor(ColorType::Shadow, {});
                mBoxShadow = {};
                // h-offset v-offset blur spread color
                bool done = false;
                uint32_t pos = 0;
                StringSpaceSkipper skipper(ruleValue);
                while (!done && skipper.cur != skipper.end) {
                    switch (pos) {
                    case 0:
                        // h-offset
                        mBoxShadow.hoffset = nameToStyleNumber(skipper.cur).value_or(StyleNumber {}).number;
                        break;
                    case 1:
                        // v-offset
                        mBoxShadow.voffset = nameToStyleNumber(skipper.cur).value_or(StyleNumber {}).number;
                        break;
                    case 2: {
                        // blur or color
                        auto blur = nameToStyleNumber(skipper.cur);
                        if (blur.has_value()) {
                            mBoxShadow.blur = blur.value().number;
                        } else {
                            auto color = parseColor(skipper.cur, skipper.next - skipper.cur);
                            if (color.has_value()) {
                                setColor(ColorType::Shadow, color.value());
                                done = true;
                            }
                        }
                        break; }
                    case 3: {
                        // spread or color
                        auto spread = nameToStyleNumber(skipper.cur);
                        if (spread.has_value()) {
                            mBoxShadow.spread = spread.value().number;
                        } else {
                            auto color = parseColor(skipper.cur, skipper.next - skipper.cur);
                            if (color.has_value()) {
                                setColor(ColorType::Shadow, color.value());
                                done = true;
                            }
                        }
                        break; }
                    case 4: {
                        // color
                        auto color = parseColor(skipper.cur, skipper.next - skipper.cur);
                        if (color.has_value()) {
                            setColor(ColorType::Shadow, color.value());
                        }
                        done = true;
                        break; }
                    }
                    ++pos;
                    skipper.advance();
                }
            }
            break; }
        case StyleRuleName::ShadowColor: {
            if (ruleValue.size() == 7 && strncasecmp(ruleValue.c_str(), "initial", 7) == 0) {
                setColor(ColorType::Shadow, {});
            } else {
                setColor(ColorType::Shadow, parseColor(ruleValue));
            }
            break; }
        case StyleRuleName::Color: {
            if (ruleValue.size() == 7 && strncasecmp(ruleValue.c_str(), "initial", 7) == 0) {
                setColor(ColorType::Foreground, {});
            } else {
                setColor(ColorType::Foreground, parseColor(ruleValue));
            }
            break; }
        case StyleRuleName::Flex: {
            // not 100% sure this is correct
            StringSpaceSkipper skipper(ruleValue);
            while (skipper.cur != skipper.end) {
                auto basis = nameToStyleFlexBasisName(skipper.cur, skipper.next - skipper.cur);
                if (std::holds_alternative<StyleNumber>(basis)) {
                    const auto& number = std::get<StyleNumber>(basis);
                    if (number.percentage) {
                        YGNodeStyleSetFlexBasisPercent(mYogaNode, number.number);
                    } else {
                        YGNodeStyleSetFlex(mYogaNode, number.number);
                    }
                } else if (std::holds_alternative<StyleFlexBasisName>(basis)) {
                    const auto bn = std::get<StyleFlexBasisName>(basis);
                    switch (bn) {
                    case StyleFlexBasisName::Initial:
                        YGNodeStyleSetFlex(mYogaNode, 0.f);
                        break;
                    case StyleFlexBasisName::Auto:
                        YGNodeStyleSetFlexBasisAuto(mYogaNode);
                        break;
                    case StyleFlexBasisName::Inherit:
                        // ### support inherit?
                        break;
                    }
                }

                skipper.advance();
            }
            break; }
        case StyleRuleName::FlexBasis: {
            auto basis = nameToStyleFlexBasisName(ruleValue.c_str(), ruleValue.size());
            if (std::holds_alternative<StyleNumber>(basis)) {
                const auto& number = std::get<StyleNumber>(basis);
                if (number.percentage) {
                    YGNodeStyleSetFlexBasisPercent(mYogaNode, number.number);
                } else {
                    YGNodeStyleSetFlexBasis(mYogaNode, number.number);
                }
            } else if (std::holds_alternative<StyleFlexBasisName>(basis)) {
                const auto bn = std::get<StyleFlexBasisName>(basis);
                switch (bn) {
                case StyleFlexBasisName::Initial:
                    YGNodeStyleSetFlexBasis(mYogaNode, 0.f);
                    break;
                case StyleFlexBasisName::Auto:
                    YGNodeStyleSetFlexBasisAuto(mYogaNode);
                    break;
                case StyleFlexBasisName::Inherit:
                    // ### support inherit?
                    break;
                }
            }
            break; }
        case StyleRuleName::FlexDirection: {
            auto dir = nameToStyleFlexDirectionName(ruleValue.c_str(), ruleValue.size());
            switch (dir) {
            case StyleFlexDirectionName::Initial:
            case StyleFlexDirectionName::Row:
                YGNodeStyleSetFlexDirection(mYogaNode, YGFlexDirectionRow);
                break;
            case StyleFlexDirectionName::RowReverse:
                YGNodeStyleSetFlexDirection(mYogaNode, YGFlexDirectionRowReverse);
                break;
            case StyleFlexDirectionName::Column:
                YGNodeStyleSetFlexDirection(mYogaNode, YGFlexDirectionColumn);
                break;
            case StyleFlexDirectionName::ColumnReverse:
                YGNodeStyleSetFlexDirection(mYogaNode, YGFlexDirectionColumnReverse);
                break;
            case StyleFlexDirectionName::Inherit:
                // ### support inherit?
                break;
            case StyleFlexDirectionName::Unknown:
                break;
            }
            break; }
        case StyleRuleName::FlexFlow: {
            // flow can be either direction or wrap
            StringSpaceSkipper skipper(ruleValue);
            while (skipper.cur != skipper.end) {
                // direction?
                auto dir = nameToStyleFlexDirectionName(skipper.cur, skipper.next - skipper.cur);
                if (dir != StyleFlexDirectionName::Unknown) {
                    switch (dir) {
                    case StyleFlexDirectionName::Initial:
                    case StyleFlexDirectionName::Row:
                        YGNodeStyleSetFlexDirection(mYogaNode, YGFlexDirectionRow);
                        break;
                    case StyleFlexDirectionName::RowReverse:
                        YGNodeStyleSetFlexDirection(mYogaNode, YGFlexDirectionRowReverse);
                        break;
                    case StyleFlexDirectionName::Column:
                        YGNodeStyleSetFlexDirection(mYogaNode, YGFlexDirectionColumn);
                        break;
                    case StyleFlexDirectionName::ColumnReverse:
                        YGNodeStyleSetFlexDirection(mYogaNode, YGFlexDirectionColumnReverse);
                        break;
                    case StyleFlexDirectionName::Inherit:
                        // ### support inherit?
                        break;
                    case StyleFlexDirectionName::Unknown:
                        break;
                    }
                } else {
                    // maybe wrap?
                    auto wrap = nameToStyleFlexWrapName(skipper.cur, skipper.next - skipper.cur);
                    switch (wrap) {
                    case StyleFlexWrapName::Wrap:
                        YGNodeStyleSetFlexWrap(mYogaNode, YGWrapWrap);
                        break;
                    case StyleFlexWrapName::WrapReverse:
                        YGNodeStyleSetFlexWrap(mYogaNode, YGWrapWrapReverse);
                        break;
                    case StyleFlexWrapName::Initial:
                        YGNodeStyleSetFlexWrap(mYogaNode, YGWrapNoWrap);
                        break;
                    case StyleFlexWrapName::Inherit:
                        // ### support inherit?
                        break;
                    case StyleFlexWrapName::Unknown:
                        break;
                    }
                }

                skipper.advance();
            }
            break; }
        case StyleRuleName::FlexGrow: {
            auto grow = nameToStyleFlexInitialInheritNumberName(ruleValue.c_str(), ruleValue.size());
            if (std::holds_alternative<StyleNumber>(grow)) {
                const auto& number = std::get<StyleNumber>(grow);
                if (!number.percentage) {
                    YGNodeStyleSetFlexGrow(mYogaNode, number.number);
                }
            } else if (std::holds_alternative<StyleFlexInitialInheritName>(grow)) {
                if (std::get<StyleFlexInitialInheritName>(grow) == StyleFlexInitialInheritName::Initial) {
                    YGNodeStyleSetFlexGrow(mYogaNode, 0.f);
                } else {
                    // ### support inherit?
                }
            }
            break; }
        case StyleRuleName::FlexShrink: {
            auto grow = nameToStyleFlexInitialInheritNumberName(ruleValue.c_str(), ruleValue.size());
            if (std::holds_alternative<StyleNumber>(grow)) {
                const auto& number = std::get<StyleNumber>(grow);
                if (!number.percentage) {
                    YGNodeStyleSetFlexShrink(mYogaNode, number.number);
                }
            } else if (std::holds_alternative<StyleFlexInitialInheritName>(grow)) {
                if (std::get<StyleFlexInitialInheritName>(grow) == StyleFlexInitialInheritName::Initial) {
                    YGNodeStyleSetFlexShrink(mYogaNode, 0.f);
                } else {
                    // ### support inherit?
                }
            }
            break; }
        case StyleRuleName::FlexWrap: {
            auto wrap = nameToStyleFlexWrapName(ruleValue.c_str(), ruleValue.size());
            switch (wrap) {
            case StyleFlexWrapName::Wrap:
                YGNodeStyleSetFlexWrap(mYogaNode, YGWrapWrap);
                break;
            case StyleFlexWrapName::WrapReverse:
                YGNodeStyleSetFlexWrap(mYogaNode, YGWrapWrapReverse);
                break;
            case StyleFlexWrapName::Initial:
                YGNodeStyleSetFlexWrap(mYogaNode, YGWrapNoWrap);
                break;
            case StyleFlexWrapName::Inherit:
                // ### support inherit?
                break;
            case StyleFlexWrapName::Unknown:
                break;
            }
            break; }
        case StyleRuleName::Gap: {
            if (ruleValue.size() == 7 && strncasecmp(ruleValue.c_str(), "initial", 7) == 0) {
                YGNodeStyleSetGap(mYogaNode, YGGutterAll, 0.f);
            } else {
                std::array<StyleNumber, 2> numbers;
                const auto numNumbers = spacedNumbers(numbers, ruleValue);
                switch (numNumbers) {
                case 1:
                    if (numbers[0].percentage) {
                        YGNodeStyleSetGapPercent(mYogaNode, YGGutterAll, numbers[0].number);
                    } else {
                        YGNodeStyleSetGap(mYogaNode, YGGutterAll, numbers[0].number);
                    }
                    break;
                case 2:
                    if (numbers[0].percentage) {
                        YGNodeStyleSetGapPercent(mYogaNode, YGGutterRow, numbers[0].number);
                    } else {
                        YGNodeStyleSetGap(mYogaNode, YGGutterRow, numbers[0].number);
                    }
                    if (numbers[1].percentage) {
                        YGNodeStyleSetGapPercent(mYogaNode, YGGutterColumn, numbers[1].number);
                    } else {
                        YGNodeStyleSetGap(mYogaNode, YGGutterColumn, numbers[1].number);
                    }
                    break;
                }
            }
            break; }
        case StyleRuleName::Margin: {
            if (ruleValue.size() == 7 && strncasecmp(ruleValue.c_str(), "initial", 7) == 0) {
                YGNodeStyleSetMargin(mYogaNode, YGEdgeAll, 0.f);
            } else {
                std::array<StyleNumber, 4> numbers;
                const auto numNumbers = spacedNumbers(numbers, ruleValue);
                switch (numNumbers) {
                case 1:
                    if (numbers[0].percentage) {
                        YGNodeStyleSetMarginPercent(mYogaNode, YGEdgeAll, numbers[0].number);
                    } else {
                        YGNodeStyleSetMargin(mYogaNode, YGEdgeAll, numbers[0].number);
                    }
                    break;
                case 2:
                    if (numbers[0].percentage) {
                        YGNodeStyleSetMarginPercent(mYogaNode, YGEdgeTop, numbers[0].number);
                        YGNodeStyleSetMarginPercent(mYogaNode, YGEdgeBottom, numbers[0].number);
                    } else {
                        YGNodeStyleSetMargin(mYogaNode, YGEdgeTop, numbers[0].number);
                        YGNodeStyleSetMargin(mYogaNode, YGEdgeBottom, numbers[0].number);
                    }
                    if (numbers[1].percentage) {
                        YGNodeStyleSetMarginPercent(mYogaNode, YGEdgeLeft, numbers[1].number);
                        YGNodeStyleSetMarginPercent(mYogaNode, YGEdgeRight, numbers[1].number);
                    } else {
                        YGNodeStyleSetMargin(mYogaNode, YGEdgeLeft, numbers[1].number);
                        YGNodeStyleSetMargin(mYogaNode, YGEdgeRight, numbers[1].number);
                    }
                    break;
                case 3:
                    if (numbers[0].percentage) {
                        YGNodeStyleSetMarginPercent(mYogaNode, YGEdgeTop, numbers[0].number);
                    } else {
                        YGNodeStyleSetMargin(mYogaNode, YGEdgeTop, numbers[0].number);
                    }
                    if (numbers[1].percentage) {
                        YGNodeStyleSetMarginPercent(mYogaNode, YGEdgeLeft, numbers[1].number);
                        YGNodeStyleSetMarginPercent(mYogaNode, YGEdgeRight, numbers[1].number);
                    } else {
                        YGNodeStyleSetMargin(mYogaNode, YGEdgeLeft, numbers[1].number);
                        YGNodeStyleSetMargin(mYogaNode, YGEdgeRight, numbers[1].number);
                    }
                    if (numbers[2].percentage) {
                        YGNodeStyleSetMarginPercent(mYogaNode, YGEdgeBottom, numbers[2].number);
                    } else {
                        YGNodeStyleSetMargin(mYogaNode, YGEdgeBottom, numbers[2].number);
                    }
                    break;
                case 4:
                    if (numbers[0].percentage) {
                        YGNodeStyleSetMarginPercent(mYogaNode, YGEdgeTop, numbers[0].number);
                    } else {
                        YGNodeStyleSetMargin(mYogaNode, YGEdgeTop, numbers[0].number);
                    }
                    if (numbers[1].percentage) {
                        YGNodeStyleSetMarginPercent(mYogaNode, YGEdgeRight, numbers[1].number);
                    } else {
                        YGNodeStyleSetMargin(mYogaNode, YGEdgeRight, numbers[1].number);
                    }
                    if (numbers[2].percentage) {
                        YGNodeStyleSetMarginPercent(mYogaNode, YGEdgeBottom, numbers[2].number);
                    } else {
                        YGNodeStyleSetMargin(mYogaNode, YGEdgeBottom, numbers[2].number);
                    }
                    if (numbers[3].percentage) {
                        YGNodeStyleSetMarginPercent(mYogaNode, YGEdgeLeft, numbers[3].number);
                    } else {
                        YGNodeStyleSetMargin(mYogaNode, YGEdgeLeft, numbers[3].number);
                    }
                    break;
                }
            }
            break; }
        case StyleRuleName::Padding: {
            if (ruleValue.size() == 7 && strncasecmp(ruleValue.c_str(), "initial", 7) == 0) {
                YGNodeStyleSetPadding(mYogaNode, YGEdgeAll, 0.f);
            } else {
                std::array<StyleNumber, 4> numbers;
                const auto numNumbers = spacedNumbers(numbers, ruleValue);
                switch (numNumbers) {
                case 1:
                    if (numbers[0].percentage) {
                        YGNodeStyleSetPaddingPercent(mYogaNode, YGEdgeAll, numbers[0].number);
                    } else {
                        YGNodeStyleSetPadding(mYogaNode, YGEdgeAll, numbers[0].number);
                    }
                    break;
                case 2:
                    if (numbers[0].percentage) {
                        YGNodeStyleSetPaddingPercent(mYogaNode, YGEdgeTop, numbers[0].number);
                        YGNodeStyleSetPaddingPercent(mYogaNode, YGEdgeBottom, numbers[0].number);
                    } else {
                        YGNodeStyleSetPadding(mYogaNode, YGEdgeTop, numbers[0].number);
                        YGNodeStyleSetPadding(mYogaNode, YGEdgeBottom, numbers[0].number);
                    }
                    if (numbers[1].percentage) {
                        YGNodeStyleSetPaddingPercent(mYogaNode, YGEdgeLeft, numbers[1].number);
                        YGNodeStyleSetPaddingPercent(mYogaNode, YGEdgeRight, numbers[1].number);
                    } else {
                        YGNodeStyleSetPadding(mYogaNode, YGEdgeLeft, numbers[1].number);
                        YGNodeStyleSetPadding(mYogaNode, YGEdgeRight, numbers[1].number);
                    }
                    break;
                case 3:
                    if (numbers[0].percentage) {
                        YGNodeStyleSetPaddingPercent(mYogaNode, YGEdgeTop, numbers[0].number);
                    } else {
                        YGNodeStyleSetPadding(mYogaNode, YGEdgeTop, numbers[0].number);
                    }
                    if (numbers[1].percentage) {
                        YGNodeStyleSetPaddingPercent(mYogaNode, YGEdgeLeft, numbers[1].number);
                        YGNodeStyleSetPaddingPercent(mYogaNode, YGEdgeRight, numbers[1].number);
                    } else {
                        YGNodeStyleSetPadding(mYogaNode, YGEdgeLeft, numbers[1].number);
                        YGNodeStyleSetPadding(mYogaNode, YGEdgeRight, numbers[1].number);
                    }
                    if (numbers[2].percentage) {
                        YGNodeStyleSetPaddingPercent(mYogaNode, YGEdgeBottom, numbers[2].number);
                    } else {
                        YGNodeStyleSetPadding(mYogaNode, YGEdgeBottom, numbers[2].number);
                    }
                    break;
                case 4:
                    if (numbers[0].percentage) {
                        YGNodeStyleSetPaddingPercent(mYogaNode, YGEdgeTop, numbers[0].number);
                    } else {
                        YGNodeStyleSetPadding(mYogaNode, YGEdgeTop, numbers[0].number);
                    }
                    if (numbers[1].percentage) {
                        YGNodeStyleSetPaddingPercent(mYogaNode, YGEdgeRight, numbers[1].number);
                    } else {
                        YGNodeStyleSetPadding(mYogaNode, YGEdgeRight, numbers[1].number);
                    }
                    if (numbers[2].percentage) {
                        YGNodeStyleSetPaddingPercent(mYogaNode, YGEdgeBottom, numbers[2].number);
                    } else {
                        YGNodeStyleSetPadding(mYogaNode, YGEdgeBottom, numbers[2].number);
                    }
                    if (numbers[3].percentage) {
                        YGNodeStyleSetPaddingPercent(mYogaNode, YGEdgeLeft, numbers[3].number);
                    } else {
                        YGNodeStyleSetPadding(mYogaNode, YGEdgeLeft, numbers[3].number);
                    }
                    break;
                }
            }
            break; }
        case StyleRuleName::Unknown:
            break;
        }
    }

    mOnAppliedStylesheet.emit();
}

void Styleable::relayout()
{
    if (!YGNodeGetHasNewLayout(mYogaNode)) {
        return;
    }

    YGNodeSetHasNewLayout(mYogaNode, false);

    for (auto child : mChildren) {
        child->relayout();
    }

    const float left = YGNodeLayoutGetLeft(mYogaNode);
    const float top = YGNodeLayoutGetTop(mYogaNode);
    const float width = YGNodeLayoutGetWidth(mYogaNode);
    const float height = YGNodeLayoutGetHeight(mYogaNode);

    const float pleft = YGNodeLayoutGetPadding(mYogaNode, YGEdgeLeft);
    const float ptop = YGNodeLayoutGetPadding(mYogaNode, YGEdgeTop);
    const float pright = YGNodeLayoutGetPadding(mYogaNode, YGEdgeRight);
    const float pbottom = YGNodeLayoutGetPadding(mYogaNode, YGEdgeBottom);

    const float mleft = YGNodeLayoutGetMargin(mYogaNode, YGEdgeLeft);
    const float mtop = YGNodeLayoutGetMargin(mYogaNode, YGEdgeTop);
    const float mright = YGNodeLayoutGetMargin(mYogaNode, YGEdgeRight);
    const float mbottom = YGNodeLayoutGetMargin(mYogaNode, YGEdgeBottom);

    // const float bleft = YGNodeLayoutGetBorder(mYogaNode, YGEdgeLeft);
    // const float btop = YGNodeLayoutGetBorder(mYogaNode, YGEdgeTop);
    // const float bright = YGNodeLayoutGetBorder(mYogaNode, YGEdgeRight);
    // const float bbottom = YGNodeLayoutGetBorder(mYogaNode, YGEdgeBottom);

    mPadding = {
        static_cast<uint32_t>(pleft),
        static_cast<uint32_t>(ptop),
        static_cast<uint32_t>(pright),
        static_cast<uint32_t>(pbottom)
    };

    mMargin = {
        static_cast<uint32_t>(mleft),
        static_cast<uint32_t>(mtop),
        static_cast<uint32_t>(mright),
        static_cast<uint32_t>(mbottom)
    };

    /*
    mBorder = {
        static_cast<uint32_t>(bleft),
        static_cast<uint32_t>(btop),
        static_cast<uint32_t>(bright),
        static_cast<uint32_t>(bbottom)
    };
    */

    spdlog::info("node {} {:.2f},{:.2f}+{:.2f}x{:.2f}",
                 mSelector.toString(),
                 left, top, width, height);

    updateLayout(Rect {
            static_cast<int32_t>(left),
            static_cast<int32_t>(top),
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        });
}
