#ifndef HALIDE_CODELOGGER_H
#define HALIDE_CODELOGGER_H

#include "IR.h"
#include <string>

namespace Halide {
namespace Internal {

class CodeLogger {
    Stmt s_prev;
    int my_section;
    std::string my_name;

public:
    // method to write stmt to a named log file.
    // If HL_LOG_FILE > 2 then the statement is logged.
    // If HL_LOG_FILE == 2 then the statement is only logged if it has changed compared to
    // the previous log.
    void log(Stmt s, std::string description);
    
    void section(int sect) { my_section = sect; }
    void name(std::string _name) { my_name = _name; }
    void reset() { s_prev = Stmt(); } // Undefined starting point.
};

extern CodeLogger code_logger;

}
}
#endif
