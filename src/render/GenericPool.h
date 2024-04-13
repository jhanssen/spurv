#pragma once

#include <array>
#include <bitset>
#include <functional>
#include <limits>
#include <optional>

namespace spurv {

struct GenericPoolImpl;

class GenericPoolBase
{
public:
    GenericPoolBase() = default;

protected:
    void runLater(std::function<void()>&& run);
    void makeAvailable(std::size_t index);

protected:
    std::bitset<32> mAvailable = {};
    std::bitset<32> mCreated = {};
};

template<typename T, std::size_t Num>
class GenericPool : public GenericPoolBase
{
public:
    GenericPool();
    ~GenericPool();

    void initialize(std::function<T()>&& creator, std::function<void(T&)>&& deletor);
    void initialize(std::function<T()>&& creator, std::function<void(T&)>&& deletor, std::function<void(T&)>&& resetor);

    class Handle
    {
    public:
        ~Handle();
        Handle(Handle&& other) = default;
        Handle& operator=(Handle&& other) = default;

        bool isValid() const;

        T operator*();
        T* operator->();
        const T* operator->() const;

    private:
        Handle() = default;
        Handle(T entry, GenericPool* pool, std::size_t index);

    private:
        T mEntry = {};
        GenericPool* mPool = nullptr;
        std::size_t mIndex = std::numeric_limits<std::size_t>::max();

        friend class GenericPool;
    };

    Handle get();
    Handle getOrCreate();

private:
    std::function<T()> mCreator;
    std::function<void(T&)> mDeletor, mResetor;

    std::array<T, Num> mData = {};

    friend class Handle;
    friend struct GenericPoolImpl;
};

template<typename T, std::size_t Num>
GenericPool<T, Num>::GenericPool()
{
    // base bitset only goes up to 32
    static_assert(Num < 32);

    // all entries are available
    mAvailable.flip();
}

template<typename T, std::size_t Num>
GenericPool<T, Num>::~GenericPool()
{
    for (std::size_t n = 0; n < Num; ++n) {
        if (mCreated.test(1 << n)) {
            mDeletor(mData[n]);
        }
    }
}

template<typename T, std::size_t Num>
void GenericPool<T, Num>::initialize(std::function<T()>&& creator, std::function<void(T&)>&& deletor)
{
    mCreator = std::move(creator);
    mDeletor = std::move(deletor);
    mResetor = {};
}

template<typename T, std::size_t Num>
void GenericPool<T, Num>::initialize(std::function<T()>&& creator, std::function<void(T&)>&& deletor, std::function<void(T&)>&& resetor)
{
    mCreator = std::move(creator);
    mDeletor = std::move(deletor);
    mResetor = std::move(resetor);
}

template<typename T, std::size_t Num>
typename GenericPool<T, Num>::Handle GenericPool<T, Num>::get()
{
    const auto avail = mAvailable.to_ullong();
    if (!avail) {
        return Handle();
    }
    const uint32_t availBit = __builtin_ctzll(avail);
    if (!mCreated.test(availBit)) {
        // create
        mData[availBit] = mCreator();
        mCreated.set(availBit);
    }
    mAvailable.reset(availBit);
    return Handle(mData[availBit], this, availBit);
}

template<typename T, std::size_t Num>
typename GenericPool<T, Num>::Handle GenericPool<T, Num>::getOrCreate()
{
    auto handle = get();
    if (handle.isValid()) {
        return handle;
    }
    T ret = mCreator();
    return Handle(ret, this, std::numeric_limits<std::size_t>::max());
}

template<typename T, std::size_t Num>
GenericPool<T, Num>::Handle::Handle(T entry, GenericPool* pool, std::size_t index)
    : mEntry(entry), mPool(pool), mIndex(index)
{
}

template<typename T, std::size_t Num>
GenericPool<T, Num>::Handle::~Handle()
{
    if (mIndex < std::numeric_limits<std::size_t>::max()) {
        if (mPool->mResetor) {
            mPool->runLater([pool = mPool, entry = std::move(mEntry), index = mIndex]() mutable {
                pool->mResetor(entry);
                pool->makeAvailable(index);
            });
        } else {
            mPool->makeAvailable(mIndex);
        }
    } else {
        mPool->runLater([pool = mPool, entry = std::move(mEntry)]() mutable {
            pool->mDeletor(entry);
        });
    }
}

template<typename T, std::size_t Num>
bool GenericPool<T, Num>::Handle::isValid() const
{
    return mPool != nullptr;
}

template<typename T, std::size_t Num>
T GenericPool<T, Num>::Handle::operator*()
{
    return mEntry;
}

template<typename T, std::size_t Num>
T* GenericPool<T, Num>::Handle::operator->()
{
    return &mEntry;
}

template<typename T, std::size_t Num>
const T* GenericPool<T, Num>::Handle::operator->() const
{
    return &mEntry;
}

} // namespace spurv
