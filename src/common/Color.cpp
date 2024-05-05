#include "Color.h"
#include <cstdlib>

namespace spurv {

std::optional<Color> parseColor(const char* color, std::size_t sz)
{
    if ((sz == 4 || sz == 5 || sz == 7 || sz == 9) && color[0] == '#') {
        char* end;
        const auto n = strtol(color + 1, &end, 16);
        if (*end == '\0') {
            switch (sz) {
            case 4:
                // #rgb
                return premultiplied(
                    ((n >> 8) & 0xf) / 255.f,
                    ((n >> 4) & 0xf) / 255.f,
                    ((n >> 0) & 0xf) / 255.f,
                    1.f
                    );
            case 5:
                return premultiplied(
                    ((n >> 12) & 0xf) / 255.f,
                    ((n >> 8) & 0xf) / 255.f,
                    ((n >> 4) & 0xf) / 255.f,
                    ((n >> 0) & 0xf) / 255.f
                    );
            case 7:
                return premultiplied(
                    ((n >> 16) & 0xff) / 255.f,
                    ((n >> 8) & 0xff) / 255.f,
                    ((n >> 0) & 0xff) / 255.f,
                    1.f
                    );
            case 9:
                return premultiplied(
                    ((n >> 24) & 0xff) / 255.f,
                    ((n >> 16) & 0xff) / 255.f,
                    ((n >> 8) & 0xff) / 255.f,
                    ((n >> 0) & 0xff) / 255.f
                    );
            }
        }
    }
    return {};
}

std::optional<Color> parseColor(const std::string& color)
{
    return parseColor(color.c_str(), color.size());
}

Color premultiplied(float r, float g, float b, float a)
{
    return Color {
        r * a,
        g * a,
        b * a,
        a
    };
}

}
