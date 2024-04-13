#include "GenericPool.h"
#include "Renderer.h"

using namespace spurv;

void GenericPoolBase::runLater(std::function<void()>&& run)
{
    Renderer::instance()->afterCurrentFrame(std::move(run));
}

void GenericPoolBase::makeAvailable(std::size_t index)
{
    Renderer::instance()->afterCurrentFrame([this, index]() {
        mAvailable.set(index);
    });
}
