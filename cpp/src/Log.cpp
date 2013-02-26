#include "Log.h"

namespace Halide {
namespace Internal {

int log::debug_level = 0;
bool log::initialized = false;
std::string log::section_name = "";
int log::section_debug_level = 0;
}
}
