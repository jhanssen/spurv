#include "AppPath.h"
#include <cassert>
#include <string>
#if defined(__APPLE__)
# include <mach-o/dyld.h>
#endif

namespace spurv {

std::filesystem::path appPath()
{
#if defined(__linux__)
    // ### should error check, follow links recursively
    return std::filesystem::read_symlink("/proc/self/exe").parent_path();
#elif defined(__APPLE__)
    {
        char buf[PATH_MAX];
        uint32_t size = sizeof(buf);
        if (_NSGetExecutablePath(buf, &size) == 0) {
            return std::filesystem::path(std::string(buf, size)).parent_path();
        }
        return ".";
    }
#else
#error Unknown platform.
#endif
}

} // namespace spurv
