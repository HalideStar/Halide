#ifndef HALIDE_LOG_H
#define HALIDE_LOG_H

/** \file
 * Defines functions for debug logging during code generation.
 */

#include <iostream>
#include "IR.h"
#include <string>
#include <set>

namespace Halide {
namespace Internal {

/** halide_option returns a string from the environment with the given name.
 * It adapts to windows and linux installations. */
std::string halide_option(std::string name);

/** For optional debugging during codegen, use the log class as
 * follows: 
 * 
 \code
 log(verbosity) << "The expression is " << expr << std::endl; 
 \endcode
 *
 * verbosity of 0 always prints, 1 should print after every major
 * stage, 2 should be used for more detail, and 3 should be used for
 * tracing everything that occurs. The verbosity with which to print
 * is determined by the value of the environment variable
 * HL_DEBUG_CODEGEN
 *
 \code
 log(verbosity,section) << ...
 \endcode
 *
 * This log message is controlled by HL_DEBUG_<section> in addition to
 * HL_DEBUG_CODEGEN.  This allows for finer control by setting an elevated
 * debug level for a particular section name.  e.g. 
 \code
   log(3,"MINE") << ... 
 \endcode
 * would print in either of the cases that
 * HL_DEBUG_CODEGEN was set to 3 or HL_DEBUG_MINE was set to 3.
 *
 \code
 log(filename,verbosity=2,section="") << ...
 \endcode
 * 
 * This log message is written to a disk file.  The first time a particular file
 * name is written to, the file is created/truncated to be empty.  The log class
 * tracks file names so that subsequent writes to the same file are appended
 * and do not overwrite the earlier write.  
 *
 * As with other forms of log, the verbosity level controls which information is
 * written to the log files.  The verbosity level for log files is the maximum of
 * HL_DEBUG_CODEGEN, HL_DEBUG_LOGFILE, and HL_DEBUG_<section>.  Thus, setting 
 * HL_DEBUG_CODEGEN to 2 will generate detailed log data and log files.
 * Setting HL_DEBUG_LOGFILE to 2 will generate the log files without the screen logs.
 * Setting HL_DEBUG_<section> to 2 will generate detailed screen logs and log files
 * for the specified section.
 *
 * The file name consists of the following parts.
 *
 * (1) The value of the environment variable HL_LOG_NAME, or if that is unset
 * then the value of the static class member log::log_name is used.
 * This allows a programmer to set log::log_name to the name of their program
 * so that the log files will be unique, but override the setting with HL_LOG_NAME.
 *
 * (2) The string passed as filename to the log constructor, which cannot be empty.  
 * (3) The file type extension ".log"
 *
 * The first two file name parts are concatenated with underscore between them,
 * except that no underscore is used if the first part is empty.
 * Only the characters a-z,A-Z,0-9 and the punctuation characters '_', '-' are 
 * allowed in the generated filename. Any other characters are changed to _.
 *
 * verbosity defaults to 2 and section defaults to "". 
 *
 * Example: Suppose a program mine is run with HL_LOG_NAME=mine.
 * Within the program, the code
 \code
   log("function_" + sobel.name()) << sobel.value();
 \endcode
 * would write the definition of the function sobel to the file
 * mine_function_sobel.log
 */

struct log {
    static int debug_level;
    static int logfile_debug_level; // Debugging level specific to log files
private:
    bool do_logging; // A boolean record of the decision to write or not to write.
    std::ostream *stream; // The output stream to use for writing.
    
    static bool initialized;
    
    static std::string section_name;
    static int section_debug_level;
    
    static std::string log_name; // Base file name for file output
    static bool log_name_env; // True if HL_LOG_BASE found in environment
    static std::set<std::string> known_files; // Files that have already been used
public:

    void constructor(std::string filename, int verbosity, std::string section = "");
    
    log(int verbosity, std::string section = "") {
        constructor("", verbosity, section);
    }
    
    log(std::string filename, int verbosity = 2, std::string section = "FILE") {
        constructor(filename, verbosity, section);
    }
    
    ~log(); // Destructor required to close file stream if used.

    template<typename T>
    log &operator<<(T x) {
        if (! do_logging) return *this;
        *stream << x;
        return *this;
    }
    
    /** Specify the default base log file name.  The
     * environment variable HL_LOG_NAME overrides this setting. */
    void set_log_name(std::string name) {
        // Only apply the programmer setting if HL_LOG_NAME was not found
        // in the environment.
        if (! log_name_env) {
            log_name = name;
        }
    }
};

}
}

#endif
