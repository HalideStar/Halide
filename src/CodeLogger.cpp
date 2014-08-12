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
    // If there was not an explicit section call, then writing code
    // automatically starts a new section.
    my_current_section = my_next_section;
    my_next_section++;
    if (! s.same_as(s_prev) || log::debug_level > 2) {
        std::ostringstream ss;
        if (description != "")
            my_description = description;
        ss << my_name << "_" << my_current_section << "_" << my_description;
        Halide::Internal::log(ss.str(), 2) << s << "\n"; 
        my_code_written = true;
    } else {
        // record that the code has not been written and should be
        // if the user writes other information to the log.
        my_code_written = false;
    }
    s_prev = s;  // Remember the code for comparison in future.
}

void CodeLogger::log(Expr e, std::string description) {
    // If there was not an explicit section call, then writing code
    // automatically starts a new section.
    my_current_section = my_next_section;
    my_next_section++;
    if (! e.same_as(e_prev) || log::debug_level > 2) {
        std::ostringstream ss;
        if (description != "")
            my_description = description;
        ss << my_name << "_" << my_current_section << "_" << my_description;
        Halide::Internal::log(ss.str(), 2) << e << "\n"; 
        my_code_written = true;
    } else {
        // record that the code has not been written and should be
        // if the user writes other information to the log.
        my_code_written = false;
    }
    e_prev = e;  // Remember the code for comparison in future.
}

Halide::Internal::log CodeLogger::log() {
    // Use the most recent section.
    // If code was not written because it was the same as earlier code, write it now.
    // You can also access the log, after changing the section, and
    // put information into it before the code gets written.
    std::ostringstream ss;
    ss << my_name << "_" << my_current_section << "_" << my_description;
    if (! my_code_written && s_prev.defined())
        Halide::Internal::log(ss.str(), 2) << s_prev << "\n";
    my_code_written = true;
    return Halide::Internal::log(ss.str(), 2);
}

}
}
