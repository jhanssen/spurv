#include "GenericPool.h"
#include "Renderer.h"

using namespace spurv;

void GenericPoolBase::runLater(std::function<void()>&& run)
{
    Renderer::instance()->afterCurrentFrame(std::move(run));
}

void GenericPoolBase::makeIndexAvailable(std::size_t index)
{
    Renderer::instance()->afterCurrentFrame([pool = this, index]() {
        pool->mAvailable.set(index);
    });
}
