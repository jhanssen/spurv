#include "Thread.h"

#if defined(__linux__)
#include <sys/prctl.h>
#elif defined(__APPLE__)
#include <pthread.h>
#endif

namespace spurv {
void setCurrentThreadName(const std::string &name)
{
#if defined(__linux__)
    prctl(PR_SET_NAME, name.c_str(), 0, 0, 0);
#elif defined(__APPLE__)
    pthread_setname_np(name.c_str());
#else
#error unsupported platform
#endif
}
} // namespace spurv
