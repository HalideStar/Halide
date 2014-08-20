#include "Log.h"
#include <ctype.h>
#include <fstream>

namespace Halide {
namespace Internal {

std::string halide_option(std::string name) {
#ifdef _WIN32
    char opt[1025];
    size_t read = 0;
    getenv_s(&read, opt, name.c_str());
    if (read) {
#else   
    if (char *opt = getenv(name.c_str())) {
#endif
        return std::string(opt);
    }
    return std::string("");
}

# define HALIDE_NO_LOGGING_LEVEL -1
int log::debug_level = HALIDE_NO_LOGGING_LEVEL;
bool log::initialized = false;

std::string log::section_name = "";
int log::section_debug_level = HALIDE_NO_LOGGING_LEVEL;

std::string log::log_name = "";
bool log::log_name_env = false; // Set true when HL_LOG_NAME found in environment
int log::logfile_debug_level = HALIDE_NO_LOGGING_LEVEL;
std::set<std::string> log::known_files; // Initially an empty set of known files

void log::constructor(std::string filename, int verbosity, std::string section) {
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
            debug_level = HALIDE_NO_LOGGING_LEVEL;
        }

        // Read the log file debug level from the environment
        #ifdef _WIN32
        char lvl[32];
        size_t read = 0;
        getenv_s(&read, lvl, "HL_DEBUG_LOGFILE");
        if (read) {
        #else   
        if (char *lvl = getenv("HL_DEBUG_LOGFILE")) {
        #endif
            logfile_debug_level = atoi(lvl);
        } else {
            logfile_debug_level = HALIDE_NO_LOGGING_LEVEL;
        }

        // Read the base log file name from the environment if specified.
        #ifdef _WIN32
        char base[32];
        size_t read = 0;
        getenv_s(&read, base, "HL_LOG_NAME");
        if (read) {
        #else   
        if (char *base = getenv("HL_LOG_NAME")) {
        #endif
            // Found environment variable to override static setting
            log_name = base;
            log_name_env = true; // Prevent programmer overriding environment
        }
        
        //std::cerr << "Logging: HL_DEBUG_CODEGEN=" << debug_level << " HL_DEBUG_LOGFILE=" << logfile_debug_level << " HL_LOG_NAME=" << log_name << "\n";

        initialized = true;
    }
    // A single cache of the section, since we assume that we are operating in stages
    if (section != "" && section != section_name) {
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
            section_debug_level = HALIDE_NO_LOGGING_LEVEL;
        }
        section_name = section;
    }
    do_logging = verbosity <= debug_level || verbosity <= section_debug_level || 
        (filename != "" && verbosity <= logfile_debug_level);
    
    stream = &(std::cerr); // Write to standard error output unless a file name is specified
    
    if (do_logging && filename != "") {
        // Write this log information to a disk file.
        std::string thename = log_name;
        if (log_name.size() > 0)
            thename = thename + "_" + filename;
        else
            thename = filename;
        // Ensure valid characters in file name.
        for (size_t i = 0; i < thename.size(); i++) {
            if (! isalnum(thename[i]) && thename[i] != '-' && thename[i] != '_') {
                thename[i] = '_';
            }
        }
        thename += ".log";
        // Determine whether file is known - already written earlier in this program
        bool known = known_files.count(thename) != 0;
        if (! known) {
            known_files.insert(thename);
        }
        stream = new std::ofstream(thename.c_str(), (known ? std::ios_base::app : std::ios_base::trunc) | std::ios_base::out);
        if (! stream) {
            // Open failed.
            std::cerr << "Attempt to open log file " << thename << " failed.\n";
            delete stream;
            stream = 0;
        }
    }
}  

log::~log() {
    if (stream != &std::cerr) { 
        delete stream; 
        stream = 0;
    }
} 

   
}
}
