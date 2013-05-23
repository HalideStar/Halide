#ifndef HALIDE_CODELOGGER_H
#define HALIDE_CODELOGGER_H

#include "IR.h"
#include "Log.h"
#include <string>

namespace Halide {
namespace Internal {

class CodeLogger {
    Stmt s_prev;
    // my_next_section: The section number to use when writing code.
    // my_current: The section number to use for everything else.
    // Note: When code is written, my_next_section is automatically
    // incremented, so each code dump goes to a separate file.
    int my_next_section, my_current_section;
    std::string my_name, my_description;
    // Code in s_prev is the most recent code section seen.
    // If my_code_written is false, then the code in s_prev is for the
    // current log (the log section my_current) but has not been written because
    // it is the same as an earlier log.  However, if additional information
    // is written to the log then the code will also be written.
    bool my_code_written;

public:
    // method to write stmt to a named log file.
    // If HL_LOG_FILE > 2 then the statement is logged.
    // If HL_LOG_FILE == 2 then the statement is only logged if it has changed compared to
    // the previous log.
    void log(Stmt s, std::string description);

    // Return the halide log file object for the most recent section
    // so that use can append additional information.
    Halide::Internal::log log();
    
    void section(int sect, std::string description = "") { 
        my_next_section = sect; /* Writing code will use this section. */
        my_current_section = sect; 
        my_code_written = true; /* Do not write the old code after changing section id */
        my_description = description; // Optional, but needed if writing other info before code
    }
    void section(std::string description = "") {
        // Start a new code log in the next section.
        my_current_section = my_next_section;
        my_code_written = true; /* Do not write old code */
        my_description = description; // Optional, but needed if writing other info before code
    }
    void name(std::string _name) { my_name = _name; }
    void reset() { s_prev = Stmt(); } // Undefined starting point.
};

extern CodeLogger code_logger;

}
}
#endif
