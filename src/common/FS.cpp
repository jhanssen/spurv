#include "FS.h"
#include <cstdio>
#include <unistd.h>
#include <sys/stat.h>

namespace spurv {
std::string readFile(const std::filesystem::path &path, bool *ok)
{
    FILE *f = fopen(path.c_str(), "r");
    if (!f) {
        if (ok)
            *ok = false;
        return std::string();
    }

    struct stat st;
    if (fstat(fileno(f), &st)) {
        if (ok) {
            *ok = false;
        }
        fclose(f);
        return {};
    }

    std::string ret;
    ret.reserve(st.st_size);
    char buf[16384];
    while (!feof(f)) {
        const ssize_t r = fread(buf, 1, sizeof(buf), f);
        ret.append(buf, r);
    }

    fclose(f);
    if (ok)
        *ok = true;
    return ret;
}

bool writeFile(const std::filesystem::path &path, const std::string &contents)
{
    FILE *f = fopen(path.c_str(), "w");
    if (!f) {
        return false;
    }

    const bool ret = fwrite(contents.c_str(), contents.size(), 1, f) == 1;
    fclose(f);
    if (!ret) {
        unlink(path.c_str());
    }

    return ret;
}
} // namespace spurv
