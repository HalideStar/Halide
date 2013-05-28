#include <stdio.h>
#include <Halide.h>
#include <string>
#include <string.h>
#include <signal.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>
#include <ctime>
#include "mtrand/mtrand.h"

#include "mtrand/mtrand.cpp"

using namespace Halide;
using namespace std;

template<typename A>
const char *string_of_type();

#define DECL_SOT(name)                                          \
    template<>                                                  \
    const char *string_of_type<name>() {return #name;}          

DECL_SOT(uint8_t);    
DECL_SOT(int8_t);    
DECL_SOT(uint16_t);    
DECL_SOT(int16_t);    
DECL_SOT(uint32_t);    
DECL_SOT(int32_t);    
DECL_SOT(float);    
DECL_SOT(double);    

#ifdef _WIN32
extern "C" bool QueryPerformanceCounter(uint64_t *);
extern "C" bool QueryPerformanceFrequency(uint64_t *);
double currentTime() {
    uint64_t t, freq;
    QueryPerformanceCounter(&t);
    QueryPerformanceFrequency(&freq);
    return (t * 1000.0) / freq;
}
#else
#include <sys/time.h>
double currentTime() {
    timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec * 1000.0 + t.tv_usec / 1000.0f;
}
#endif


# define WIDTH 1024
# define HEIGHT 1024

/** Schedule options */
// Apply ISS
# define SCHEDULE_SPLIT_INDEX 1
// Apply bound
# define SCHEDULE_BOUND 2
// Apply compute_root to the border expression
# define SCHEDULE_ROOT_BORDER 4
// Use an image parameter instead of an image
# define SCHEDULE_INPUT_PARAM 8
// Apply compute_root to the first dimension before 2nd dimension processing
# define SCHEDULE_ROOT_DIM_1 16
// Make y parallel in blocks
// Three bits are allocated for codes.
# define SCHEDULE_PARALLEL (32|64|128)
# define SCHEDULE_PARALLEL_NONE 0
# define SCHEDULE_PARALLEL_1 32
# define SCHEDULE_PARALLEL_4 64
# define SCHEDULE_PARALLEL_16 96
// Upcast the data for processing, and downcast it afterwards
// Two bits are allocated for codes.
# define SCHEDULE_UPCAST (256|512)
# define SCHEDULE_UPCAST_NONE 0
# define SCHEDULE_UPCAST_SAME 256
# define SCHEDULE_UPCAST_INT 512
# define SCHEDULE_UPCAST_FLOAT (512|256)
// Apply root to the upcast of the border handled input
# define SCHEDULE_ROOT_UPCAST 1024


/** Function stages */
// The main (last) function 
# define FUNC_MAIN 1
// The first dimension (when a separable computation is performed)
# define FUNC_DIM_1 2
// The border handler
# define FUNC_BORDER 3
// The upcast
# define FUNC_UPCAST 4

# define BORDER_CLAMP 0
# define BORDER_MOD 1

#ifndef HALIDE_NEW
# define HALIDE_NEW 0
#endif
#ifndef HALIDE_VERSION
# define HALIDE_VERSION 0
#endif

// Support for auto partitioning of loops.
# define HAS_PARTITION (HALIDE_NEW && HALIDE_VERSION > 130422)
# define HAS_STATISTICS (HALIDE_NEW && HALIDE_VERSION > 130412)
# define HAS_COMPILE_STMT ((HALIDE_NEW && HALIDE_VERSION >= 130509) || (! HALIDE_NEW))

// Set to true when logging/debugging code generation - each function will only be compiled and executed once.
int logging = 0;
std::string model = "unknown";
std::string version = "unknown";

// Set to true to list test numbers and names.
int listing = 0;

// Set true to list test id numbers only.
int list_ids = 0;
std::vector<int> ids;

// Test ID passed as parameter; -1 means run all, but a timeout will abandon subsequent tests.
// To continue past timeout, you have to execute the program specifically for each individual test
int testid = -1;

// Which group of experiments to perform
int experiment_group = 0;

// Set with -t option to limit time allowed for each individual compilation, in seconds.
int timelimit = 20;

// A class object that wraps up managing data collection
class DataCollector {
    std::ofstream out_file; // The output file when opened.
    std::map<std::string,int> data_map; // Map recording number of times each experiment is already recorded in file
    
    void do_write(std::ostream &out, double jit, double stmt, double execute, int hits, int misses, bool incr) {
        std::string id = data_id();
        time_t now;
        time(&now);
        char buf[sizeof "1999-12-31T23:59:59      "];
        strftime(buf, sizeof buf, "%FT%T", localtime(&now));
        out << id << ",@," << std::setiosflags(std::ios::fixed) << std::setprecision(3) << jit << "," 
                 << stmt << "," << execute << ","
                 << hits << "," << misses << "," << buf << std::endl;
        out.flush();
        if (incr)
            data_count_incr(id);
    }
    
    void data_count_incr(std::string &id) {
        std::map<std::string,int>::iterator it;
        it = data_map.find(id);
        if (it == data_map.end()) {
            data_map[id] = 1;
        } else {
            it->second++;
        }
        //std::cerr << "Data " << id << " == " << data_map[id] << "\n";
    }

public:
    int max_data_count; // Maximum count of each data item that is required. From -n option on command line.
    std::string filename; // Data file name, from -f option on command line.
    
    // Data parameters.  In the order that they are set by set_data_info() below.
    std::string data_type; // Data type u8, i32, f32, etc.
    std::string base_name; // Base name of the generated function.
    int parameter_n; // Parameter specifying size of the generated function.  e.g. conv(n)
    std::string schedule_bound, schedule_splitindex, schedule_root_border, schedule_root_dim_1; // Schedule options
    int schedule_vector, schedule_parallel_y, schedule_unroll_x; 
    std::string schedule_upcast, schedule_root_upcast;
    std::string border; // The type of border.  Such as clamp, mod, etc.
    std::string input_type; // image or param
    int dimensionality; // 1 or 2 D function
    std::string name; // The name of the generated function.
    
    // Priority selection
 
    
    DataCollector() {
        max_data_count = 1;
        filename = "";
    }
    
    void set_data_info(Type t, std::string basename, int n, int bdr, int schedule, int vec, int dimensions) {
        // NOTE: To prevent data interpretation errors, unimplemented features must NOT set option variables here.
        // However, all implemented features must be fully implemented and MUSE set option variables here.
        
        std::ostringstream ss;
        if (t.is_uint()) ss << "u" << t.bits;
        if (t.is_int()) ss << "i" << t.bits;
        if (t.is_float()) ss << "f" << t.bits;
        
        // Set data_type and base_name
        data_type = ss.str();
        base_name = basename;
        ss << "_" << basename;
        
        // Set dimensionality
        dimensionality = dimensions;
        if (dimensionality == 1) { ss << "_1d"; }
        if (dimensionality == 2) { ss << "_2d"; }
        
        // Set parameter_n
        if (n > 0) ss << "_" << n;
        parameter_n = n;
        
        // Set schedule options
        if (schedule & SCHEDULE_BOUND) { ss << "_b"; schedule_bound = "bound"; } else { schedule_bound = "nobound"; }
        if (schedule & SCHEDULE_SPLIT_INDEX) { ss << "_s"; schedule_splitindex = "iss"; } else { schedule_splitindex = "noiss"; }
        if (schedule & SCHEDULE_ROOT_BORDER) { ss << "_rb"; schedule_root_border = "rootborder"; } else { schedule_root_border = "norootborder"; }
        schedule_root_dim_1 = "norootdim1";
        if (schedule & SCHEDULE_ROOT_DIM_1) { ss << "_r1"; schedule_root_dim_1 = "rootdim1"; } else { schedule_root_dim_1 = "norootdim1"; }
        if (vec > 0) ss << "_v" << vec;
        schedule_vector = vec;
        schedule_parallel_y = 0;
        // Unimplemented...
        //int s = schedule * SCHEDULE_PARALLEL
        //if (s == SCHEDULE_PARALLEL_1) { ss << "_p1"; schedule_parallel_y = 1; }
        //if (s == SCHEDULE_PARALLEL_4) { ss << "_p4"; schedule_parallel_y = 4; }
        //if (s == SCHEDULE_PARALLEL_16) { ss << "_p16"; schedule_parallel_y = 16; }
        schedule_unroll_x = 0; // Not yet implemented.  No unrolling is default.
        
        int s = schedule & SCHEDULE_UPCAST;
        if (s == SCHEDULE_UPCAST_NONE) { schedule_upcast = "noup"; }
        else if (s == SCHEDULE_UPCAST_SAME) { ss << "_us"; schedule_upcast = "upsame"; }
        else if (s == SCHEDULE_UPCAST_INT) { ss << "_ui"; schedule_upcast = "upint"; }
        else if (s == SCHEDULE_UPCAST_FLOAT) { ss << "_uf"; schedule_upcast = "upfloat"; }
        if (schedule & SCHEDULE_ROOT_UPCAST) { ss << "_rup"; schedule_root_upcast = "rootup"; } else { schedule_root_upcast = "norootup"; }
        
        // Set border
        if (bdr == BORDER_CLAMP) { ss << "_clamp"; border = "clamp"; }
        else if (bdr == BORDER_MOD) { ss << "_mod"; border = "mod"; }
        else { assert(0 && "Unknown border mode"); }
        
        // Set input_type
        input_type = "image";
        // Unimplemented...
        //if (schedule & SCHEDULE_INPUT_PARAM) { ss << "_param"; input_type = "param"; } else { input_type = "image"; }
        
        // Set name
        name = ss.str();
    }
    
    // Return the ID string associated with an experiment.
    std::string data_id() {
        std::ostringstream ss;
        ss << model << "," << version << "," << name << ","
           << data_type << "," << base_name << "," << parameter_n << "," << dimensionality << ","
           << schedule_root_dim_1 << ","
           << border << "," << schedule_bound << "," << schedule_splitindex << ","
           << schedule_root_border << "," << schedule_vector << "," << input_type << ","
           << schedule_parallel_y << "," << schedule_unroll_x << ","
           << schedule_upcast << "," << schedule_root_upcast << ","
           << "," << "," << "," << 0 << "," << 0 << "," << 0; // Future expansion - three string and three int fields.
        return ss.str();
    }

    // Return the number of times this experiment has been recorded.
    int data_count() {
        std::string id = data_id();
        
        std::map<std::string,int>::iterator it;
        it = data_map.find(id);
        if (it == data_map.end()) {
            return 0;
        } else {
            return it->second;
        }
    }
    
    // Open the output file.
    void open() {
        out_file.open(filename.c_str(), std::ofstream::out | std::ofstream::app);
        if (! out_file.is_open()) {
            std::cerr << "ERROR: Could not open output file " << filename << "\n";
            filename = "";
        }
    }
    
    // Write data to the output file, or to stdout if output file is not defined.
    void write_data(double jit, double stmt, double execute, int hits, int misses) {
        if (filename == "") {
            do_write(std::cout, jit, stmt, execute, hits, misses, false);
        } else {
            do_write(out_file, jit, stmt, execute, hits, misses, true);
        }
    }

    // Pre-read the data file and count data ids into the map.
    void read_file() {
        std::ifstream in_file;
        std::string line;
        in_file.open(filename.c_str(), std::ifstream::in);
        if (! in_file.is_open()) {
            std::cerr << "Warning: " << filename << " does not exist and will be created.\n";
            return; // File does not exist.
        }
        while (! in_file.eof()) {
            getline(in_file, line);
            if (line.length() == 0)
                continue;
            // Look for the data_id delimiter.  This string is magic!
            size_t found = line.find(",@,");
            if (found == std::string::npos) {
                // Not found,  skip this apparently invalid line
                std::cerr << "Warning: Skipping line (no ID delimiter)\n";
                std::cerr << "    " << line << "\n";
                continue;
            }
            // The ID is the first found characters of line.
            std::string id = line.substr(0, found);
            data_count_incr(id);
        }
        in_file.close();
    }
};


DataCollector collector; // The one object for data collection

class PriorityManager {
    map<string,int> data_type;
    map<string,int> base_name;
    map<int,int> parameter_n;
    map<string,int> schedule_bound;
    map<string,int> schedule_splitindex;
    map<string,int> schedule_root_border;
    map<string,int> schedule_root_dim_1;
    map<int,int> schedule_vector;
    map<int,int> schedule_parallel_y;
    map<int,int> schedule_unroll_x;
    map<string,int> border;
    map<int,int> dimensionality;
    
    int priority;
    
    // Priorities are 1<<first_priority, 1<<(first_priority-1), ...
    // If not found but required, returns neg_priority which is
    // big enough that it overrides all other positive priorities added together
    // but small enough that you can add it together first_priority times without overflow.
    const int first_priority;
    const int neg_priority;
    
public:
    PriorityManager() : first_priority(24), neg_priority((1<<first_priority)*-2) {
        priority = first_priority;
    }
    
    int next_priority() { assert(priority > 0); return (1 << priority--); }
    
    template<typename T>
    int lookup(map<T,int> m, T key) {
        typename map<T,int>::iterator it;
        it = m.find(key);
        // If required but not found, return neg_priority.
        if (it == m.end()) return m.size() > 0 ? neg_priority : 0; 
        return it->second;
    }
    
    // Return the priority of the current item. 0 means not selected.  1 means default selected.
    int p(DataCollector &c) {
        int pri = 0;
        pri += lookup(data_type, c.data_type);
        pri += lookup(base_name, c.base_name);
        //std::cerr << "Looking up n " << c.parameter_n << " ==> " << lookup(parameter_n, c.parameter_n) << "\n";
        pri += lookup(parameter_n, c.parameter_n);
        pri += lookup(schedule_bound, c.schedule_bound);
        pri += lookup(schedule_splitindex, c.schedule_splitindex);
        pri += lookup(schedule_root_border, c.schedule_root_border);
        pri += lookup(schedule_root_dim_1, c.schedule_root_dim_1);
        pri += lookup(schedule_vector, c.schedule_vector);
        pri += lookup(schedule_parallel_y, c.schedule_parallel_y);
        pri += lookup(schedule_unroll_x, c.schedule_unroll_x);
        pri += lookup(border, c.border);
        pri += lookup(dimensionality, c.dimensionality);
        if (pri < 0) return 0;
        if (pri == 0) return 1; // All default selections.
        return pri;
    }
    
    int stoi(string s) {
        int v = atoi(s.c_str());
        return v;
    }
    
    int option(string opt, string param) {
        if (opt == "-type") {
            if (param == "f32" || param == "f64" || param == "i32" || param == "i16" || param == "i8" ||
                param == "u32" || param == "u16" || param == "u8") {
                data_type[param] = next_priority();
                return 1; // Used the param
            } else {
                std::cerr << "Unknown data type " << param << "\n";
                assert(0);
            }
        } else if (opt == "-f32") { data_type["f32"] = next_priority(); 
        } else if (opt == "-f64") { data_type["f64"] = next_priority(); 
        } else if (opt == "-i32") { data_type["i32"] = next_priority();
        } else if (opt == "-i16") { data_type["i16"] = next_priority(); 
        } else if (opt == "-i8") { data_type["i8"] = next_priority(); 
        } else if (opt == "-u32") { data_type["u32"] = next_priority(); 
        } else if (opt == "-u16") { data_type["u16"] = next_priority();
        } else if (opt == "-u8") { data_type["u8"] = next_priority();
        } else if (opt == "-func") {
            assert(param != "" && "Empty parameter to -func");
            base_name[param] = next_priority();
            return 1;
        } else if (opt == "-blur") { base_name["blur"] = next_priority(); 
        } else if (opt == "-conv") { base_name["conv"] = next_priority(); 
        } else if (opt == "-diag") { base_name["diag"] = next_priority(); 
        } else if (opt == "-sobel") { base_name["sobel"] = next_priority(); 
        } else if (opt == "-n") {
            int n = stoi(param);
            assert(n > 0);
            parameter_n[n] = next_priority();
            return 1;
        } else if (opt == "-bound") {
            schedule_bound["bound"] = next_priority();
        } else if (opt == "-nobound") {
            schedule_bound["nobound"] = next_priority();
        } else if (opt == "-iss") {
            schedule_splitindex["iss"] = next_priority();
        } else if (opt == "-noiss") {
            schedule_splitindex["noiss"] = next_priority();
        } else if (opt == "-rootborder" || opt == "-rb") {
            schedule_root_border["rootborder"] = next_priority();
        } else if (opt == "-norootborder" || opt == "-norb") {
            schedule_root_border["norootborder"] = next_priority();
        } else if (opt == "-rootdim1" || opt == "-r1") {
            schedule_root_border["rootdim1"] = next_priority();
        } else if (opt == "-norootdim1" || opt == "-nor1") {
            schedule_root_border["norootdim1"] = next_priority();
        } else if (opt == "-vector" || opt == "-vec") {
            int vec = stoi(param);
            schedule_vector[vec] = next_priority();
            return 1;
        } else if (opt == "-parallel" || opt == "-p") {
            int par = stoi(param);
            schedule_parallel_y[par] = next_priority();
            return 1;
        } else if (opt == "-unroll" || opt == "-u") {
            int un = stoi(param);
            schedule_unroll_x[un] = next_priority();
            return 1;
        } else if (opt == "-border") {
            assert(param != "" && "Empty parameter to -border");
            border[param] = next_priority();
            return 1;
        } else if (opt == "-clamp") { border["clamp"] = next_priority(); 
        } else if (opt == "-mod") { border["mod"] = next_priority(); 
        } else if (opt == "-dim") {
            int dim = stoi(param);
            dimensionality[dim] = next_priority();
            return 1;
        } else if (opt == "-1d") { dimensionality[1] = next_priority(); 
        } else if (opt == "-2d") { dimensionality[2] = next_priority(); 
        } else if (opt[0] == '-') {
            std::cerr << "Unknown option " << opt << "\n";
            assert(0);
        }
        return 0;
    }
};

PriorityManager priority;

class Random {
    MTRand_int32 rand;
public:
    Random() {
        seed(time(NULL));
    }
    
    void seed(unsigned int s) {
        if (s == 0)
            rand.seed (time(NULL));
        else
            rand.seed(s);
        for (int i = 0; i < 100; i++)
            rand(); // Discard some random numbers.
    }
    
    int uniform(int n) {
        return (rand() % (unsigned int) n);
    }
    
    void shuffle(std::vector<int> &v) {
        // Shuffle by randomised swap.
        for (size_t i = 1; i < v.size(); i++) {
            // Loop invariant: v[0:i-1] is shuffled
            // Element i is to be shuffled into a randomly
            // selected position on the interval [0,i].
            int shufflepos = uniform(i+1);
            //std::cerr << "Swap " << i << " " << shufflepos << "\n";
            std::swap(v[i], v[shufflepos]);
            // Since elements [0,i-1] were randomly shuffled,
            // whatever element we swap out to position i is still
            // randomised.
            // v[0:i] is now shuffled
        }
    }
};

Random mt_random;

void alarm_handler(int sig) {
    signal(SIGALRM, SIG_IGN);
    fprintf (stderr, "%s: Timeout\n", collector.name.c_str());
    collector.write_data(-1.0, -1.0, -1.0, 0, 0);
    exit(0);
}

static Var x("x"), y("y"), yi("yi");

int dim(int n, int dimensionality) {
    if (dimensionality <= 1) return n;
    if (dimensionality == 2) {
        if (n == 9) return 3;
        if (n == 16) return 4;
        if (n == 25) return 5;
        if (n == 36) return 6;
        if (n == 49) return 7;
        if (n == 64) return 8;
        if (n == 81) return 9;
        if (n == 100) return 10;
        if (n == 121) return 11;
        if (n == 144) return 12;
    }
    std::cerr << "Invalid n " << n << " for dimensionality " << dimensionality << "\n";
}

#define MAX_UNIQUE 200
#define MIN_UNIQUE 11
class UniqueInt {
public:
    
    bool unique_delivered[MAX_UNIQUE+1];
    Random u_random;

    void reset(int seed) {
        for (int i = 0; i < MAX_UNIQUE+1; i++)
            unique_delivered[i] = 0;
        u_random.seed(seed);
    }

    int i() {
        // Generate unique integers that are not too small and not too large.
        int k;
        int failcount = 0;
        while (1) {
            k = u_random.uniform(MAX_UNIQUE-MIN_UNIQUE) + MIN_UNIQUE;
            if (unique_delivered[k] == 0 && unique_delivered[k-1] == 0 && unique_delivered[k+1] == 0)
                break; // Highly successful - no immediate neighbours.
            failcount++;
            // After 100 failures, allow neighbours
            if (failcount > 100 && unique_delivered[k] == 0)
                break;
            if (failcount > 1000) {
                std::cerr << "unique_int: seems to be exhausted\n";
                return k;
            }
        }
        unique_delivered[k] = 1;
        return k;
    }
};

UniqueInt unique;

void do_schedule(Func f, int stage, int schedule, int vec) {
    if (schedule & SCHEDULE_BOUND) f.bound(x,0,WIDTH).bound(y,0,HEIGHT);
    if (vec > 0) f.vectorize(x, vec);
    int sp = schedule & SCHEDULE_PARALLEL;
    if (sp == SCHEDULE_PARALLEL_1) {
        f.parallel(y);
    } else if (sp == SCHEDULE_PARALLEL_4) {
        f.split(y, y, yi, 4).parallel(y);
    } else if (sp == SCHEDULE_PARALLEL_16) {
        f.split(y, y, yi, 16).parallel(y);
    }
# if HAS_PARTITION
    if (schedule & SCHEDULE_SPLIT_INDEX) f.partition(); // Implicitly, this is partition all
# else
    assert ((schedule & SCHEDULE_SPLIT_INDEX) == 0 && "Schedule split index not supported");
# endif
    int do_root = 0;
    switch (stage) {
        case FUNC_MAIN:
            do_root = 0;
            break;
        case FUNC_DIM_1:
            do_root = schedule & SCHEDULE_ROOT_DIM_1;
            break;
        case FUNC_BORDER:
            do_root = schedule & SCHEDULE_ROOT_BORDER;
            break;
        case FUNC_UPCAST:
            do_root = schedule & SCHEDULE_ROOT_UPCAST;
            break;
    }
    if (do_root) f.compute_root();
}

// The schedule bits that are handled by calling do_schedule for FUNC_MAIN
# define SCHEDULE_DO_MAIN (SCHEDULE_BOUND|SCHEDULE_SPLIT_INDEX|SCHEDULE_PARALLEL)
// The same for DIM_1 that are in addition to the above
# define SCHEDULE_DO_DIM_1 (SCHEDULE_ROOT_DIM_1)

template<typename T>
Func build_basic(std::string name, ImageParam a, Image<T> b, int n, int bdr, int schedule, int vec, int dimensionality) {
    Func f(name);
    assert(n == 0 && "Func basic does not support n != 0");
    assert(bdr == 0 && "Func basic does not support bdr != 0");
    assert(schedule == 0 && "Func basic does not support schedule != 0");
    assert(dimensionality == 1 && "Func basic does not support dimensionality other than 1");
    f(x) = a(x) + b(x);
    do_schedule(f, FUNC_MAIN, schedule, vec);
    return f;
}

template<typename T>
Func build_simplify_n(std::string name, ImageParam a, Image<T> b, int n, int bdr, int schedule, int vec, int dimensionality) {
    assert(dimensionality == 1 && "Func simplify_n does not support dimensionality other than 1");
    assert((schedule & SCHEDULE_BOUND) == schedule && "Func simplify_n does not support schedule other than bound");
    assert(bdr == 0 && "Func simplify_n does not support border");
    assert(vec == 0 && "Func simplify_n does not support vectorisation");
    Func f(name);
    unique.reset(n);
    Expr e = a(x,y) * unique.i();
    for (int i = 1; i < n; i++) {
        e = e + a(x,y) * unique.i();
    }
    f(x,y) = e;
    do_schedule(f, FUNC_MAIN, schedule, vec);
    return f;
}

// Upcast a function of type t under control of schedule.  Vectorisation is done if specified.
Func upcast(Func in, Type t, int schedule, int vec) {
    Func upcast("upcast");
    int s = schedule & SCHEDULE_UPCAST;
    if (s == SCHEDULE_UPCAST_NONE) {
        assert ((schedule & SCHEDULE_ROOT_UPCAST) == 0 && "Root upcast incompatible with no upcast");
        return in; // No upcasting.
    }
    // Apart from SCHEDULE_UPCAST_NONE, an actual upcast must occur.
    // We dont want things that appear different to actually be the same.
    if (s == SCHEDULE_UPCAST_FLOAT) {
        assert(! t.is_float() && "Cannot upcast float to float");
        int bits = t.bits;
        if (bits < 32) bits = 32;
        upcast = cast(Float(bits), in);
    } else if (s == SCHEDULE_UPCAST_INT) {
        assert(! t.is_float() && "Cannot upcast float to int");
        assert(t.bits < 64 && "Cannot upcast 64 bits");
        int bits = t.bits * 2;
        upcast = cast(Int(bits), in);
    } else if (s == SCHEDULE_UPCAST_SAME) {
        assert(! t.is_float() && "Cannot upcast float to same");
        assert(t.bits < 64 && "Cannot upcast 64 bits");
        Type u = t;
        int bits = t.bits * 2;
        u.bits = bits;
        upcast = cast(u, in);
    }
    do_schedule(upcast, FUNC_UPCAST, schedule, vec);
    return upcast;
}

// Downcast an expression to type t.  If it is already of type t, nothing happens.
Expr downcast(Type t, Expr e) {
    if (e.type() != t)
        return cast(t, e);
    else
        return e;
}

// Border handling itself is not vectorized, but upcast can be.
template<typename T>
Func border_handled(Image<T> b, int bdr, int schedule, int vec) {
    Func border("border");
    if (bdr == BORDER_CLAMP) {
        border(x,y) = b(clamp(x,0,WIDTH-1), clamp(y,0,HEIGHT-1));
    } else if (bdr == BORDER_MOD) {
        border(x,y) = b(x%WIDTH, y%HEIGHT);
    } else {
        assert(0 && "Invalid bdr selector");
    }
    do_schedule(border, FUNC_BORDER, schedule, 0); // Border handler is not vectorized
    // Upcast may be performed.
    Func up = upcast(border, type_of<T>(), schedule, vec);
    return up;
}

// All the schedule bits that are (almost fully) handled by border_handled
# define SCHEDULE_BORDER_HANDLED (SCHEDULE_ROOT_BORDER|SCHEDULE_UPCAST|SCHEDULE_ROOT_UPCAST)

Expr subscript(Func in, int xdelta, int ydelta) {
    if (xdelta == 0 && ydelta == 0) return in(x, y);
    if (ydelta == 0) return in(x + xdelta, y);
    if (xdelta == 0) return in(x, y + ydelta);
    return in(x + xdelta, y + ydelta);
}

Expr blur_pixel(Func in, int xdelta, int ydelta) {
    return subscript(in, xdelta, ydelta);
}

Expr blur_final(Expr e, int n) {
    return e / n;
}

Expr conv_pixel(Func in, int xdelta, int ydelta) {
    return subscript(in, xdelta, ydelta) * unique.i();
}

Expr conv_final(Expr e, int n) {
    return e;
}

template<typename T>
Func build_general(Expr (*pixel)(Func,int,int), Expr (*final)(Expr,int), std::string name, ImageParam a, Image<T> b, int n, int bdr, int schedule, int vec, int dimensionality) {
    assert((schedule & (SCHEDULE_DO_MAIN | SCHEDULE_BORDER_HANDLED | SCHEDULE_DO_DIM_1)) == schedule && 
           "build_general does not support schedule");
    Func h("h"), f(name);
    Func in = border_handled(b, bdr, schedule, vec);
    // Unique number generation.
    // Depends on size n of the operator and on the iteration number during data collection.
    // However, for a particular iteration, all programs will be identical.
    // Also, note that the same numbers will be generated for different programs.
    // And, finally, that the same numbers will be generated for different dimensionality
    // if n is the same.
    unique.reset(n * 1000 + collector.data_count());
    int m = dim(n, dimensionality);
    Expr e = (*pixel)(in,-m/2,0);   // e.g. in(x-m/2, y)
    for (int i = 1; i < m; i++) {
        e = e + (*pixel)(in,i-m/2,0);  // e.g. in(x+(i-m/2), y)
    }
    if (dimensionality == 2) {
        h(x,y) = e;
        Expr ev = (*pixel)(h,0,-m/2); // e.g. h(x,y-m/2)
        for (int i = 1; i < m; i++) {
            ev = ev + (*pixel)(h,0,i-m/2); // e.g. h(x,y+(i-m/2))
        }
        f(x,y) = downcast(type_of<T>(), (*final)(ev, n));  // e.g. ev / n
        do_schedule(h, FUNC_DIM_1, schedule, vec);
    } else {
        f(x,y) = downcast(type_of<T>(), (*final) (e, n));  // e.g. e / n
    }
    do_schedule(f, FUNC_MAIN, schedule, vec);
    return f;
}

template<typename T>
Func build_blur_n(std::string name, ImageParam a, Image<T> b, int n, int bdr, int schedule, int vec, int dimensionality) {
    return build_general(blur_pixel, blur_final, name, a, b, n, bdr, schedule, vec, dimensionality);
}

template<typename T>
Func build_conv_n(std::string name, ImageParam a, Image<T> b, int n, int bdr, int schedule, int vec, int dimensionality) {
    return build_general(conv_pixel, conv_final, name, a, b, n, bdr, schedule, vec, dimensionality);
}

template<typename T>
Func build_sobel(std::string name, ImageParam a, Image<T> b, int n, int bdr, int schedule, int vec, int dimensionality) {
    assert(dimensionality == 2 && "Sobel is only defined for dimensionality 2");
    assert((schedule & (SCHEDULE_BORDER_HANDLED | SCHEDULE_DO_MAIN)) == schedule &&
        "Func sobel does not implement schedule");
    Func in = border_handled(b, bdr, schedule, vec); // Data type upcast can be done if required.
    //std::cerr << in << "\n";
    Func h("h"), v("v"), sobel("sobel");
    h(x,y) = in(x-1,y-1) + 2 * in(x-1,y) + in(x-1,y+1) - in(x+1,y-1) - 2 * in(x+1,y) - in(x+1,y+1);
    v(x,y) = in(x-1,y-1) + 2 * in(x,y-1) + in(x+1,y-1) - in(x-1,y+1) - 2 * in(x,y+1) - in(x+1,y+1);
    sobel(x,y) = downcast(type_of<T>(), (abs(h(x,y)) + abs(v(x,y)))/4);
    do_schedule(sobel, FUNC_MAIN, schedule, vec);
    return sobel;
}

template<typename T>
Func build_diag_n(std::string name, ImageParam a, Image<T> b, int n, int bdr, int schedule, int vec, int dimensionality) {
    assert(dimensionality == 2 && "diag is only defined for dimensionality 2");
    assert((schedule & (SCHEDULE_BORDER_HANDLED | SCHEDULE_DO_MAIN)) == schedule &&
        "Func diag does not implement schedule");
    Func f(name);
    Func in = border_handled(b, bdr, schedule, vec);
    f(x,y) = downcast(type_of<T>(), in(x-n,y-n) + in(x+n,y+n));
    do_schedule(f, FUNC_MAIN, schedule, vec);
    return f;
}

int new_check(int check, double MinMeasureTime, double delta, int max) {
    int c = check * (MinMeasureTime * 1.5) / delta;
    if (c <= check) c = check + 1;
    if (c >= max) c = max;
    return c;
}

// Counts the calls to test, so each test gets a unique ID.
int testcount = 0;
// Tracks the maximum priority so that we can focus on the most important tests.
int max_priority = 0;
// Tracks the minimum data count so we can focus on missing data.
int min_data_count = (1<<30); // A big enough number

template<typename T>
void test(std::string basename, 
        Func (*builder)(std::string name, ImageParam, Image<T>, int, int, int, int, int), 
        int n, int bdr, int schedule, int vec, int dimensionality, int vec_only) {
    if (vec_only && vec == 0) return; // This is not even a test - it would be a repeat.
    collector.set_data_info(type_of<T>(), basename, n, bdr, schedule, vec, dimensionality);
    
    testcount++;
    
    // Check to see whether this test has already been done sufficient times.
    // Only works if data file has been selected with -f option.
    if (! logging) {
        int dc = collector.data_count();
        if (dc >= collector.max_data_count)
            return;
        if (dc < min_data_count) {
            min_data_count = dc;
            ids.clear(); // Forget any accumulated ids with higher data count.
        }
    }
    
    // Looking at tests that do not have enough data, compute the priority.
    // If the priority is zero, the test is deselected.
    int pri = priority.p(collector);
    if (pri == 0) return; // Not selected by priority options
    
    // If the priority is higher than seen before (and requiring data to be collected)
    // then only accept tests of that priority or higher.  Works best with accumulated ids.
    if (! logging) {
        if (pri > max_priority) {
            max_priority = pri;
            ids.clear(); // Forget any accumulated ids of lower priority
        }
        if (pri < max_priority) return; // Not selected based on priority
        if (listing) {
            std::cout << testcount << " " << collector.name << "\n";
            return;
        }
        if (list_ids) {
            ids.push_back(testcount);
            return;
        }
    }

    if (testcount != testid && testid != -1) return; // Skip this test.  ID -1 means do all.
    
    ImageParam a(type_of<T>(), 2);
    Image<T> b(WIDTH,HEIGHT), c(WIDTH,HEIGHT), d(WIDTH,HEIGHT), ref(WIDTH,HEIGHT);
    a.set(c);
    
    // Initialise the images with pseudo-data
    for (int i = 0; i < WIDTH; i++) {
        for (int j = 0; j < HEIGHT; j++) {
            b(i,j) = (i * 123 + j * 11) % 1023 + i;
            c(i,j) = (i + j * WIDTH) * 3;
            d(i,j) = 0;
        }
    }
    
    double t1, t2;
    Func f;

    int count, check = 1;
    alarm(timelimit);
    signal(SIGALRM, alarm_handler);
    const double MinMeasureTime = 2000.0;
    int max_jit = logging ? 1 : 100;
    t1 = currentTime();
    for (count = 0; ; ) {
        count++;
        f = (*builder)(collector.name, a, b, n, bdr, schedule, vec, dimensionality);
        f.compile_jit();
        if (count >= check) {
            t2 = currentTime(); // Record the end time.
            double delta = t2 - t1;
            if (delta > MinMeasureTime || count >= max_jit)
                break;
            if (delta < MinMeasureTime / 10.0) delta = MinMeasureTime / 10.0;
            if (check == 1) {
                alarm(0);
            }
            check = new_check(check, MinMeasureTime, delta, max_jit);
            t1 = currentTime();
            count = 0;
        }
    }    

    double jit = (t2 - t1) / count;
    
    alarm(0);
    
    fprintf (stderr, "%s: %.1f ms per jit\n", collector.name.c_str(), jit);
    
    double stmt = 0.0;

# if HAS_COMPILE_STMT
    if (logging) {
        stmt = 0.0;
    } else {
        check = 1;
        int max_stmt = logging ? 1 : 1000;
        t1 = currentTime();
        for (count = 0; ; ) {
            count++;
            (*builder)(collector.name, a, b, n, bdr, schedule, vec, dimensionality).compile_to_stmt();
            if (count >= check) {
                t2 = currentTime(); // Record the end time.
                double delta = t2 - t1;
                if (delta > MinMeasureTime || count >= max_stmt)
                    break;
                if (delta < MinMeasureTime / 10.0) delta = MinMeasureTime / 10.0;
                check = new_check(check, MinMeasureTime, delta, max_stmt);
                t1 = currentTime();
                count = 0;
            }
        }    

        stmt = (t2 - t1) / count;
    }
# endif

    fprintf (stderr, "%s: %.3f ms per lower\n", collector.name.c_str(), stmt);
    
    // Execute once.
    f.realize(d);
    
    if (schedule != 0 && ! logging) {
        // Compute the preferred output using the builder with no
        // schedule.  Except: preserve upcast because that is necessary for correct results. 
        // Also preserve SCHEDULE_INPUT_PARAM.
        (*builder)(collector.name, a, b, n, bdr, schedule & (SCHEDULE_UPCAST | SCHEDULE_INPUT_PARAM), 0, dimensionality).realize(ref);
        
        for (int i = 0; i < WIDTH; i++) {
            for (int j = 0; j < HEIGHT; j++) {
                if (ref(i,j) != d(i,j)) {
                    bool error = true;
                    if (type_of<T>().is_float()) error = std::abs(ref(i,j) - d(i,j)) > std::abs(ref(i,j)) * 0.00001;
                    if (error) {
                    std::cerr << "Incorrect result from " << collector.name << " with schedule " << schedule << "\n";
                    std::cerr << "At (" << i << ", " << j << "): Expected " << ref(i,j) << "  Result was " << d(i,j) << "\n";
                    assert(0); // Failure of the test.
                    }
                }
            }
        }
    }
    // Now to estimate execution time.
    check = logging ? 1 : 4;
    int max_exec = logging ? 1 : 1000000;
    t1 = currentTime();
    for (count = 0; ; ) {
        count++;
        f.realize(d);
        if (count >= check) {
            t2 = currentTime(); // Record the end time.
            double delta = t2 - t1;
            if (delta > MinMeasureTime || count >= max_exec)
                break;
            if (delta < MinMeasureTime / 10.0) delta = MinMeasureTime / 10.0;
            check = new_check(check, MinMeasureTime, delta, max_exec);
            t1 = currentTime();
            count = 0;
        }
    }
    double execute = (t2 - t1) / count;
    
    int hits = 0, misses = 0;
# if HAS_STATISTICS
    hits = global_statistics.mutator_cache_hits;
    misses = global_statistics.mutator_cache_misses;
# endif
    fprintf (stderr, "%s: %.3f ms per execution\n", collector.name.c_str(), execute);
    collector.write_data(jit, stmt, execute, hits, misses);
}

template<typename T>
void do_conv_n(int bdr, int schedule, int vec, int dimensionality, int vec_only) {
    if (dimensionality == 1) {
        test<T>("conv", build_conv_n, 3, bdr, schedule, vec, dimensionality, vec_only);
        test<T>("conv", build_conv_n, 5, bdr, schedule, vec, dimensionality, vec_only);
    }
    test<T>("conv", build_conv_n, 9, bdr, schedule, vec, dimensionality, vec_only);
    test<T>("conv", build_conv_n, 16, bdr, schedule, vec, dimensionality, vec_only);
    test<T>("conv", build_conv_n, 25, bdr, schedule, vec, dimensionality, vec_only);
    test<T>("conv", build_conv_n, 49, bdr, schedule, vec, dimensionality, vec_only);
    test<T>("conv", build_conv_n, 81, bdr, schedule, vec, dimensionality, vec_only);
}

template<typename T>
void do_blur_n(int bdr, int schedule, int vec, int dimensionality, int vec_only) {
    if (dimensionality == 1) {
        test<T>("blur", build_blur_n, 3, bdr, schedule, vec, dimensionality, vec_only);
        test<T>("blur", build_blur_n, 5, bdr, schedule, vec, dimensionality, vec_only);
    }
    test<T>("blur", build_blur_n, 9, bdr, schedule, vec, dimensionality, vec_only);
    test<T>("blur", build_blur_n, 16, bdr, schedule, vec, dimensionality, vec_only);
    test<T>("blur", build_blur_n, 25, bdr, schedule, vec, dimensionality, vec_only);
    test<T>("blur", build_blur_n, 49, bdr, schedule, vec, dimensionality, vec_only);
    test<T>("blur", build_blur_n, 81, bdr, schedule, vec, dimensionality, vec_only);
}

template<typename T>
void do_diag(int bdr, int vec, int dimensionality, int vec_only) {
    const int max_diag = 16;
    for (int k = 1; k <= max_diag; k++)
        test<T>("diag", build_diag_n, k, bdr, 0, 0, dimensionality, vec_only);
    for (int k = 1; k <= max_diag; k++)
        test<T>("diag", build_diag_n, k, bdr, 0, vec, dimensionality, vec_only);
# if HAS_PARTITION
    for (int k = 1; k <= max_diag; k++)
        test<T>("diag", build_diag_n, k, bdr, SCHEDULE_SPLIT_INDEX, 0, dimensionality, vec_only);
    for (int k = 1; k <= max_diag; k++)
        test<T>("diag", build_diag_n, k, bdr, SCHEDULE_SPLIT_INDEX, vec, dimensionality, vec_only);
# endif
    for (int k = 1; k <= max_diag; k++)
        test<T>("diag", build_diag_n, k, bdr, SCHEDULE_BOUND, 0, dimensionality, vec_only);
    for (int k = 1; k <= max_diag; k++)
        test<T>("diag", build_diag_n, k, bdr, SCHEDULE_BOUND, vec, dimensionality, vec_only);
# if HAS_PARTITION
    for (int k = 1; k <= max_diag; k++)
        test<T>("diag", build_diag_n, k, bdr, SCHEDULE_BOUND | SCHEDULE_SPLIT_INDEX, 0, dimensionality, vec_only);
    for (int k = 1; k <= max_diag; k++)
        test<T>("diag", build_diag_n, k, bdr, SCHEDULE_BOUND | SCHEDULE_SPLIT_INDEX, vec, dimensionality, vec_only);
# endif
}

template<typename T>
void do_sobel(int bdr, int vec, int dimensionality, int vec_only) {
    int up[] = { SCHEDULE_UPCAST_INT, SCHEDULE_UPCAST_FLOAT };
    for (int i = 0; i < 3; i++) {
        if (up[i] == SCHEDULE_UPCAST_FLOAT && type_of<T>().is_float()) continue; // Skip this case
        test<T>("sobel", build_sobel, 9, bdr, up[i], 0, dimensionality, vec_only);
        test<T>("sobel", build_sobel, 9, bdr, up[i], vec, dimensionality, vec_only);
        test<T>("sobel", build_sobel, 9, bdr, up[i] | SCHEDULE_BOUND, 0, dimensionality, vec_only);
        test<T>("sobel", build_sobel, 9, bdr, up[i] | SCHEDULE_BOUND, vec, dimensionality, vec_only);
# if HAS_PARTITION
        test<T>("sobel", build_sobel, 9, bdr, up[i] | SCHEDULE_SPLIT_INDEX, 0, dimensionality, vec_only);
        test<T>("sobel", build_sobel, 9, bdr, up[i] | SCHEDULE_SPLIT_INDEX, vec, dimensionality, vec_only);
        test<T>("sobel", build_sobel, 9, bdr, up[i] | SCHEDULE_BOUND | SCHEDULE_SPLIT_INDEX, 0, dimensionality, vec_only);
        test<T>("sobel", build_sobel, 9, bdr, up[i] | SCHEDULE_BOUND | SCHEDULE_SPLIT_INDEX, vec, dimensionality, vec_only);
# endif
    }
}

template<typename T>
void do_tests(int bdr, int vec, int dimensionality, int vec_only = 0) {
    do_blur_n<T>(bdr, 0, 0, dimensionality, vec_only);
    do_blur_n<T>(bdr, SCHEDULE_ROOT_BORDER, 0, dimensionality, vec_only);
    do_blur_n<T>(bdr, 0, vec, dimensionality, vec_only);
    do_blur_n<T>(bdr, SCHEDULE_ROOT_BORDER, vec, dimensionality, vec_only);
# if HAS_PARTITION
    do_blur_n<T>(bdr, SCHEDULE_SPLIT_INDEX, 0, dimensionality, vec_only);
    do_blur_n<T>(bdr, SCHEDULE_SPLIT_INDEX | SCHEDULE_ROOT_BORDER, 0, dimensionality, vec_only);
    do_blur_n<T>(bdr, SCHEDULE_SPLIT_INDEX, vec, dimensionality, vec_only);
    do_blur_n<T>(bdr, SCHEDULE_SPLIT_INDEX | SCHEDULE_ROOT_BORDER, vec, dimensionality, vec_only);
# endif

    for (int sbound = 0; sbound < 2; sbound++) {
	int sb = sbound * SCHEDULE_BOUND;
        for (int srootborder = 0; srootborder < 2; srootborder++) {
            int srb = srootborder * SCHEDULE_ROOT_BORDER | sb;
            for (int dovec = 0; dovec < 2; dovec++) {
                int vv = dovec * vec;
                do_conv_n<T>(bdr, srb, vv, dimensionality, vec_only);
# if HAS_PARTITION
                do_conv_n<T>(bdr, srb | SCHEDULE_SPLIT_INDEX, vv, dimensionality, vec_only);
# endif
            }
        }
    }

    if (dimensionality == 2) {
	do_diag<T>(bdr, vec, dimensionality, vec_only);
        do_sobel<T>(bdr, vec, dimensionality, vec_only);
    }

    if (dimensionality == 1) {
        test<T>("simplify", build_simplify_n, 5, bdr, 0, 0, dimensionality, vec_only);
        test<T>("simplify", build_simplify_n, 10, bdr, 0, 0, dimensionality, vec_only);
        test<T>("simplify", build_simplify_n, 20, bdr, 0, 0, dimensionality, vec_only);
        test<T>("simplify", build_simplify_n, 40, bdr, 0, 0, dimensionality, vec_only);
        test<T>("simplify", build_simplify_n, 80, bdr, 0, 0, dimensionality, vec_only);
    }
}

// An experiment group that is very brief - use it to check out this program.
template<typename T>
void do_tests_brief(int bdr, int vec, int dimensionality, int vec_only = 0) {
    const int max_diag = 3;
    for (int k = 1; k <= max_diag; k++)
        test<T>("diag", build_diag_n, k, bdr, 0, vec, dimensionality, vec_only);
}

template<typename T>
void do_tests_minus2(int bdr, int vec, int dimensionality, int vec_only = 0) {
    do_conv_n<T>(bdr, 0, vec, dimensionality, vec_only);
}


int main(int argc, char **argv) {
    // Command line information
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-log") == 0) logging = 1; // Compile and execute each function once only.
        else if (strcmp(argv[i], "-list") == 0) listing = 1; // List IDs and function names. Useful to focus on a test.
        else if (strcmp(argv[i], "-t") == 0 && i < argc - 1) { // Limit compilation time in seconds.
            // Parse time limit.
            sscanf(argv[i+1], "%d", &timelimit);
            i++;
        } else if (strcmp(argv[i], "-cpu") == 0 && i < argc - 1) { // Specify CPU model string
            model = argv[i+1];
            i++;
        } else if (strcmp(argv[i], "-v") == 0 && i < argc - 1) { // Specify Halide version string
            version = argv[i+1];
            i++;
        } else if (strcmp(argv[i], "-f") == 0 && i < argc - 1) { // Data file name for automatic data collection
            collector.filename = argv[i+1];
            collector.read_file();
            collector.open();
            i++;
        } else if (strcmp(argv[i], "-count") == 0 && i < argc - 1) { // Number of repetitions required for data collection
            sscanf(argv[i+1], "%d", &collector.max_data_count);
            i++;
        } else if (strcmp(argv[i], "-ids") == 0) { // List the IDs of experiments waiting to be performed
            list_ids = 1; 
        } else if (strcmp(argv[i], "-exp") == 0 && i < argc - 1) { // Select experiment group number.
            sscanf(argv[i+1], "%d", &experiment_group);
            i++;
        } else if (argv[i][0] == '-') {
            if (i < argc - 1) {
                if (priority.option(argv[i], argv[i+1])) i++;
            } else {
                priority.option(argv[i], "");
            }
        } else {
            // Parse test ID number to be run.
            sscanf(argv[i], "%d", &testid);
        }
    }
    
    // Randomised memory allocation
    char *memory_allocated_1 = NULL, *memory_allocated_2 = NULL;
    if (! list_ids) {
        int size = mt_random.uniform(1<<10) << 10;
        memory_allocated_1 = new char[size];
        size = mt_random.uniform(1<<10);
        memory_allocated_2 = new char[size];
    }
    
    if (experiment_group == 0) {
        for (int dim = 1; dim <= 2; dim++) {
            do_tests<uint8_t>(BORDER_CLAMP, 8, dim);
            do_tests<uint8_t>(BORDER_MOD, 8, dim);
            //do_tests<uint8_t>(BORDER_CLAMP, 16, dim, 1);
            //do_tests<uint8_t>(BORDER_MOD, 16, dim, 1);
            do_tests<float>(BORDER_CLAMP, 4, dim);
            do_tests<float>(BORDER_MOD, 4, dim);
            do_tests<int>(BORDER_CLAMP, 4, dim);
            do_tests<int>(BORDER_MOD, 4, dim);
            do_tests<uint16_t>(BORDER_CLAMP, 8, dim);
            do_tests<uint16_t>(BORDER_MOD, 8, dim);
        }
    }
    
    if (experiment_group == -1) {
        do_tests_brief<uint8_t>(BORDER_CLAMP, 8, 1);
    }
    
    if (experiment_group == -2) {
        do_tests_minus2<float>(BORDER_CLAMP, 4, 1);
    }

    if (list_ids) {
        // Shuffle the collected IDs to randomise the experiments
        mt_random.shuffle(ids);
        for (size_t i = 0; i < ids.size(); i++) std::cout << ids[i] << " ";
        std::cout << std::endl;
    }
    
    return 0;
}
