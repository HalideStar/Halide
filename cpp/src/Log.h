#ifndef HALIDE_LOG_H
#define HALIDE_LOG_H

/** \file
 * Defines functions for debug logging during code generation.
 */

#include <iostream>
#include "IR.h"
#include <string>

namespace Halide {
namespace Internal {

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
 * log(verbosity,section) << ...
 * This log message is controlled by HL_DEBUG_<section> in addition to
 * HL_DEBUG_CODEGEN.  This allows for finer control by setting an elevated
 * debug level for a particular section name.  e.g. 
 * log(3,"MINE") << ... would print the output in either of the cases that
 * HL_DEBUG_CODEGEN was set to 3 or HL_DEBUG_MINE was set to 3.
 */

struct log {
    static int debug_level;
    static bool initialized;
    //int verbosity;
    bool do_logging; // A boolean to avoid repeating the decision.
    static std::string section_name;
    static int section_debug_level;

    log(int verbosity) {
        if (!initialized) {
            // Read the debug level from the environment
            #ifdef _WIN32
            char lvl[32];
            size_t read = 0;
            getenv_s(&read, lvl, "HL_DEBUG_CODEGEN");
            if (read) {
            #else   
            if (char *lvl = getenv("HL_DEBUG_CODEGEN")) {
            #endif
                debug_level = atoi(lvl);
            } else {
                debug_level = 0;
            }
            initialized = true;
        }
        do_logging = verbosity <= debug_level;
    }

    log(int verbosity, std::string section) {
        if (!initialized) {
            // Read the debug level from the environment
            #ifdef _WIN32
            char lvl[32];
            size_t read = 0;
            getenv_s(&read, lvl, "HL_DEBUG_CODEGEN");
            if (read) {
            #else   
            if (char *lvl = getenv("HL_DEBUG_CODEGEN")) {
            #endif
                debug_level = atoi(lvl);
            } else {
                debug_level = 0;
            }

            initialized = true;
        }
        // A single cache of the section, since we assume that we are operating in stages
        if (section != section_name)
        {
            // Read the debug level from the environment
            #ifdef _WIN32
            char lvl[32];
            size_t read = 0;
            getenv_s(&read, lvl, ("HL_DEBUG_" + section).c_str()));
            if (read) {
            #else   
            if (char *lvl = getenv(("HL_DEBUG_" + section).c_str())) {
            #endif
                section_debug_level = atoi(lvl);
            } else {
                section_debug_level = 0;
            }
            section_name = section;
        }
        do_logging = verbosity <= debug_level || verbosity <= section_debug_level;
    }

    template<typename T>
    log &operator<<(T x) {
        if (! do_logging) return *this;
        std::cerr << x;
        return *this;
    }
};

}
}

#endif
