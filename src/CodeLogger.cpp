#include "CodeLogger.h"
#include "Log.h"
#include "IRPrinter.h"
#include "IREquality.h"
#include <sstream>
#include <iostream>

namespace Halide {
namespace Internal {

// The global code logger.
CodeLogger code_logger;

void CodeLogger::log(Stmt s, std::string description) {
    if (! s.same_as(s_prev) || log::debug_level > 2) {
        std::ostringstream ss;
        ss << my_name << "_" << my_section << "_" << description;
        Halide::Internal::log(ss.str(), 2) << s << "\n"; 
        s_prev = s; 
    }
    my_section++;
}

}
}
