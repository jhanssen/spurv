#include "text/Font.h"
#include <fmt/core.h>

using namespace spurv;

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    Font font("Corsiva");
    fmt::print("hello world {}\n", font.file().string());
}
