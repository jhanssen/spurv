#pragma once

#include <Styleable.h>
#include <memory>
#include <cstddef>
#include <cstdint>

namespace spurv {

class View;

class Cursor : public Styleable
{
public:
    Cursor();
    Cursor(const std::shared_ptr<View>& view);
    Cursor(Cursor&&) = default;
    virtual ~Cursor() override;

    Cursor& operator=(Cursor&&) = default;

    void setView(const std::shared_ptr<View>& doc);
    const std::shared_ptr<View> view() const;

    void setOffset(std::size_t cluster);
    void setPosition(std::size_t line, uint32_t cluster);

    std::size_t offset() const;
    std::size_t line() const;
    uint32_t cluster() const;

    enum class Navigate {
        ClusterForward,
        ClusterBackward,
        LineUp,
        LineDown,
        LineStart,
        LineEnd,
        WordForward,
        WordBackward
    };
    bool navigate(Navigate nav);

    void insert(char32_t unicode);

protected:
    virtual void updateLayout(const Rect&) override { }

private:
    std::size_t mLine = {};
    uint32_t mCluster = {}, mRetainedCluster = {};

    std::shared_ptr<View> mView = {};
};

inline const std::shared_ptr<View> Cursor::view() const
{
    return mView;
}

inline std::size_t Cursor::line() const
{
    return mLine;
}

inline uint32_t Cursor::cluster() const
{
    return mCluster;
}

} // namespace spurv
