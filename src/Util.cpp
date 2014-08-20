#include "Util.h"
#include <sstream>
#include <map>

namespace Halide { 
namespace Internal {

using std::string;
using std::vector;
using std::ostringstream;
using std::map;

// Similar functions specifically for conveniently constructing vectors of strings from literals
std::vector<std::string> vecS(std::string a) {
    std::vector<std::string> v(1);
    v[0] = a;
    return v;
}

std::vector<std::string> vecS(std::string a, std::string b) {
    std::vector<std::string> v(2);
    v[0] = a;
    v[1] = b;
    return v;
}

std::vector<std::string> vecS(std::string a, std::string b, std::string c) {
    std::vector<std::string> v(3);
    v[0] = a;
    v[1] = b;
    v[2] = c;
    return v;
}

std::vector<std::string> vecS(std::string a, std::string b, std::string c, std::string d) {
    std::vector<std::string> v(4);
    v[0] = a;
    v[1] = b;
    v[2] = c;
    v[3] = d;
    return v;
}


string unique_name(char prefix) {
    // arrays with static storage duration should be initialized to zero automatically
    static int instances[256]; 
    ostringstream str;
    str << prefix << instances[(unsigned char)prefix]++;
    return str.str();
}

bool starts_with(const string &str, const string &prefix) {
    if (str.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); i++) {
        if (str[i] != prefix[i]) return false;
    }
    return true;
}

bool ends_with(const string &str, const string &suffix) {
    if (str.size() < suffix.size()) return false;
    size_t off = str.size() - suffix.size();
    for (size_t i = 0; i < suffix.size(); i++) {
        if (str[off+i] != suffix[i]) return false;
    }
    return true;
}

string unique_name(const string &name) {
    static map<string, int> known_names;

    // If the programmer specified a single character name then use the
    // pre-existing Halide unique name generator.
    if (name.length() == 1) {
        return unique_name(name[0]);
    }    

    // An empty string really does not make sense, but use 'z' as prefix.
    if (name.length() == 0) {
        return unique_name('z');
    }    

    // Check the '$' character doesn't appear in the prefix. This lets
    // us separate the name from the number using '$' as a delimiter,
    // which guarantees uniqueness of the generated name, without
    // having to track all name generated so far.
    // Retain the name that the programmer assigned to this Func object.

    // If the name has a dollar sign in it, drop the end.  This will return the original
    // programmer supplied name and then the unique_name will append a new numeric suffix.
    std::string thename = name;
    size_t pos = thename.find('$');
    if (pos != std::string::npos) {
        thename = thename.substr(0, pos);
    }
    
    int &count = known_names[thename];
    count++;
    if (count == 1) {
        // The very first unique name is the original function name itself.
        return thename;
    } else {
        // Use the programmer-specified name but append a number to make it unique.
        ostringstream oss;        
        oss << thename << '$' << count;
        return oss.str();
    }
}

string base_name(const string &name) {
    size_t off = name.rfind('.');
    if (off == string::npos) {
        return "";
    }
    return name.substr(off+1);
}

}
}
