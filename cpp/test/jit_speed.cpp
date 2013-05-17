#include <stdio.h>
#include <Halide.h>
#include <string.h>
#include <signal.h>
#include <ostream>

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

using namespace Halide;

# define WIDTH 1024
# define HEIGHT 1024

# define SCHEDULE_SPLIT_INDEX 1
# define SCHEDULE_BOUND 2
# define SCHEDULE_ROOT_BORDER 4

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

// Test ID passed as parameter; -1 means run all, but a timeout will abandon subsequent tests.
int testid = -1;

// Set with -t option to limit time allowed for each individual compilation, in seconds.
int timelimit = 20;
std::string the_name;
std::string data_type;
std::string base_name;
int parameter_n;
int schedule_bound, schedule_splitindex, schedule_vector, schedule_root_border;
std::string border_mode;

void print_data(double jit, double stmt, double execute, int hits, int misses) {
    printf ("%s,%s,%s,%s,%s,%d,%s,%d,%d,%d,%d,%.1f,%.1f,%.1f,%d,%d\n", model.c_str(), version.c_str(), the_name.c_str(), 
            data_type.c_str(), base_name.c_str(), parameter_n, border_mode.c_str(), schedule_bound, schedule_splitindex, 
            schedule_root_border, schedule_vector, jit, stmt, execute, hits, misses);
    fflush (stdout);
}

void alarm_handler(int sig) {
    signal(SIGALRM, SIG_IGN);
    fprintf (stderr, "%s: Timeout\n", the_name.c_str());
    print_data(-1.0, -1.0, -1.0, 0, 0);
    exit(0);
}

static Var x("x"), y("y");


template<typename T>
Func build_basic(std::string name, ImageParam a, Image<T> b, int n, int bdr, int schedule, int vec) {
    Func f(name);
    f(x) = a(x) + b(x);
    if (vec > 0) f.vectorize(x,vec);
    return f;
}

template<typename T>
Func border_handled(Image<T> b, int bdr, int schedule) {
    Func border("border");
    if (bdr == BORDER_CLAMP) {
        border(x,y) = b(clamp(x,0,WIDTH-1), clamp(y,0,HEIGHT-1));
    } else if (bdr == BORDER_MOD) {
        border(x,y) = b(x%WIDTH, y%HEIGHT);
    } else {
        assert(0 && "Invalid bdr selector");
    }
    if (schedule & SCHEDULE_ROOT_BORDER) {
        border.compute_root();
        // No point in applying bound to the border handled because
        // the whole idea is to make it bigger, and I would need to specify how much bigger
# if HAS_PARTITION
        if (schedule & SCHEDULE_SPLIT_INDEX) border.partition();
# endif
    return border;
}

template<typename T>
Func build_blur(std::string name, ImageParam a, Image<T> b, int n, int bdr, int schedule, int vec) {
    Func h("h"), f(name);
    Func border = border_handled(b, bdr, schedule);
    h(x,y) = border(x-1,y) + border(x,y) + border(x+1,y);
    f(x,y) = (h(x,y-1) + h(x,y) + h(x,y+1)) / 9;
    if (schedule & SCHEDULE_BOUND) f.bound(x,0,WIDTH).bound(y,0,HEIGHT);
    if (vec > 0) f.vectorize(x,vec);
# if HAS_PARTITION
    if (schedule & SCHEDULE_SPLIT_INDEX) f.partition();
# endif
    return f;
}

template<typename T>
Func build_simplify_n(std::string name, ImageParam a, Image<T> b, int n, int bdr, int schedule, int vec) {
    Func f(name);
    int mult = 23;
    Expr e = a(x,y) * mult;
    for (int i = 1; i < n; i++) {
        mult = mult + (i % 7) + ((i * i) % 3) + 1; // Monotonic increasing multipliers, randomised
        e = e + a(x,y) * mult;
    }
    f(x,y) = e;
    if (schedule & SCHEDULE_BOUND) f.bound(x,0,WIDTH).bound(y,0,HEIGHT);
    return f;
}

template<typename T>
Func build_conv_n(std::string name, ImageParam a, Image<T> b, int n, int bdr, int schedule, int vec) {
    Func f(name);
    int mult = 23;
    Func in = border_handled(b, bdr, schedule);
    Expr e = in(x-n/2,y) * mult;
    for (int i = 1; i < n; i++) {
        mult = mult + (i % 7) + ((i * i) % 3) + 1; // Monotonic increasing multipliers, randomised
        e = e + in(x+(i-n/2),y) * mult;
    }
    f(x,y) = e;
    if (schedule & SCHEDULE_BOUND) f.bound(x,0,WIDTH).bound(y,0,HEIGHT);
    if (vec > 0) f.vectorize(x,vec);
# if HAS_PARTITION
    if (schedule & SCHEDULE_SPLIT_INDEX) f.partition();
# endif
    return f;
}

template<typename T>
Func build_diag_n(std::string name, ImageParam a, Image<T> b, int n, int bdr, int schedule, int vec) {
    Func f(name);
    int mult = 23;
    Func in = border_handled(b, bdr, schedule);
    f(x,y) = in(x-n,y-n) + in(x+n,y+n);
    if (schedule & SCHEDULE_BOUND) f.bound(x,0,WIDTH).bound(y,0,HEIGHT);
    if (vec > 0) f.vectorize(x,vec);
# if HAS_PARTITION
    if (schedule & SCHEDULE_SPLIT_INDEX) f.partition();
# endif
    return f;
}

// Counts the calls to test, so each test gets a unique ID.
int testcount = 0;

template<typename T>
void test(std::string basename, 
        Func (*builder)(std::string name, ImageParam, Image<T>, int, int, int, int), 
        int n, int bdr, int schedule, int vec, int vec_only) {
    if (vec_only && vec == 0) return; // This is not even a test - it would be a repeat.
    std::ostringstream ss;
    if (type_of<T>().is_uint()) ss << "u" << type_of<T>().bits;
    if (type_of<T>().is_int()) ss << "i" << type_of<T>().bits;
    if (type_of<T>().is_float()) ss << "f" << type_of<T>().bits;
    data_type = ss.str();
    base_name = basename;
    ss << "_" << basename;
    if (n > 0) ss << "_" << n;
    parameter_n = n;
    if (schedule & SCHEDULE_BOUND) { ss << "_b"; schedule_bound = 1; } else { schedule_bound = 0; }
    if (schedule & SCHEDULE_SPLIT_INDEX) { ss << "_s"; schedule_splitindex = 1; } else { schedule_splitindex = 0; }
    if (schedule & SCHEDULE_ROOT_BORDER) { ss << "_r"; schedule_root_border = 1; } else { schedule_root_border = 0; }
    if (vec > 0) ss << "_v" << vec;
    if (bdr == BORDER_CLAMP) { ss << "_clamp"; border_mode = "clamp"; }
    else if (bdr == BORDER_MOD) { ss << "_mod"; border_mode = "mod"; }
    else { assert(0 && "Unknown border mode"); }
    schedule_vector = vec;
    std::string name = ss.str();
    
    testcount++;
    if (listing) {
        std::cout << testcount << " " << name << "\n";
        return;
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
    t1 = currentTime();

    Func f;

    int count, check = 1;
    the_name = name;
    alarm(timelimit);
    signal(SIGALRM, alarm_handler);
    for (count = 0; count < (logging ? 1 : 100); count++) {
        f = (*builder)(name, a, b, n, bdr, schedule, vec);
        f.compile_jit();
        if (count >= check) {
            t2 = currentTime();
            if (t2 - t1 > 2000.0)
                break;
            if (check == 1) {
                alarm(0);
                check = 4;
            } else check = check * 2;
        }
    }    

    t2 = currentTime();
    double jit = (t2 - t1) / count;
    
    alarm(0);
    
    fprintf (stderr, "%s: %.1f ms per jit\n", name.c_str(), jit);
    
    double stmt = 0.0;

# if HAS_COMPILE_STMT
    if (logging) {
        stmt = 0.0;
    } else {
        t1 = currentTime();
        check = 1;
        for (count = 0; count < (logging ? 1 : 100); count++) {
            (*builder)(name, a, b, n, bdr, schedule, vec).compile_to_stmt();
            if (count >= check) {
                t2 = currentTime();
                if (t2 - t1 > 2000.0)
                    break;
                if (check == 1) {
                    alarm(0);
                    check = 4;
                } else check = check * 2;
            }
        }    

        t2 = currentTime();
        stmt = (t2 - t1) / count;
    }
# endif

    fprintf (stderr, "%s: %.1f ms per lower\n", name.c_str(), stmt);
    
    // Execute once.
    f.realize(d);
    
    if (schedule != 0 && ! logging) {
        // Compute the preferred output using the builder with no
        // schedule.
        (*builder)(name, a, b, n, bdr, 0, 0).realize(ref);
        
        for (int i = 0; i < WIDTH; i++) {
            for (int j = 0; j < HEIGHT; j++) {
                if (ref(i,j) != d(i,j)) {
                    bool error = true;
                    if (type_of<T>().is_float()) error = std::abs(ref(i,j) - d(i,j)) > std::abs(ref(i,j)) * 0.00001;
                    if (error) {
                    std::cerr << "Incorrect result from " << name << " with schedule " << schedule << "\n";
                    std::cerr << "At (" << i << ", " << j << "): Expected " << ref(i,j) << "  Result was " << d(i,j) << "\n";
                    assert(0); // Failure of the test.
                    }
                }
            }
        }
    }
    // Now to estimate execution time.
    t1 = currentTime();
    check = 4;
    for (count = 0; count < (logging ? 1 : 1000); count++) {
        f.realize(d);
        if (count >= check) {
            t2 = currentTime();
            if (t2 - t1 > 2000.0)
                break;
            check = check * 2;
        }
    }
    
    t2 = currentTime();
    double execute = (t2 - t1) / count;
    
    int hits = 0, misses = 0;
# if HAS_STATISTICS
    hits = global_statistics.mutator_cache_hits;
    misses = global_statistics.mutator_cache_misses;
# endif
    fprintf (stderr, "%s: %.1f ms per execution\n", name.c_str(), execute);
    print_data(jit, stmt, execute, hits, misses);
    fflush(stdout);
}

template<typename T>
void do_conv_n(int bdr, int schedule, int vec, int vec_only) {
    test<T>("conv", build_conv_n, 3, bdr, schedule, vec, vec_only);
    test<T>("conv", build_conv_n, 5, bdr, schedule, vec, vec_only);
    test<T>("conv", build_conv_n, 10, bdr, schedule, vec, vec_only);
    test<T>("conv", build_conv_n, 20, bdr, schedule, vec, vec_only);
    test<T>("conv", build_conv_n, 40, bdr, schedule, vec, vec_only);
    test<T>("conv", build_conv_n, 80, bdr, schedule, vec, vec_only);
}

template<typename T>
void do_tests(int bdr, int vec, int vec_only = 0) {
    //test<T>("basic", build_basic, 0, bdr);
    //test<T>("basic", build_basic, 0, bdr, 0, vec);
    const int max_diag = 2;
    for (int k = 1; k <= max_diag; k++)
        test<T>("diag", build_diag_n, k, bdr, 0, 0, vec_only);
    for (int k = 1; k <= max_diag; k++)
        test<T>("diag", build_diag_n, k, bdr, 0, vec, vec_only);
# if HAS_PARTITION
    for (int k = 1; k <= max_diag; k++)
        test<T>("diag", build_diag_n, k, bdr, SCHEDULE_SPLIT_INDEX, 0, vec_only);
    for (int k = 1; k <= max_diag; k++)
        test<T>("diag", build_diag_n, k, bdr, SCHEDULE_SPLIT_INDEX, vec, vec_only);
# endif
    for (int k = 1; k <= max_diag; k++)
        test<T>("diag", build_diag_n, k, bdr, SCHEDULE_BOUND, 0, vec_only);
    for (int k = 1; k <= max_diag; k++)
        test<T>("diag", build_diag_n, k, bdr, SCHEDULE_BOUND, vec, vec_only);
# if HAS_PARTITION
    for (int k = 1; k <= max_diag; k++)
        test<T>("diag", build_diag_n, k, bdr, SCHEDULE_BOUND | SCHEDULE_SPLIT_INDEX, 0, vec_only);
    for (int k = 1; k <= max_diag; k++)
        test<T>("diag", build_diag_n, k, bdr, SCHEDULE_BOUND | SCHEDULE_SPLIT_INDEX, vec, vec_only);
# endif
    test<T>("blur", build_blur, 0, bdr, 0, 0, vec_only);
    test<T>("blur", build_blur, 0, bdr, 0, vec, vec_only);
# if HAS_PARTITION
    test<T>("blur", build_blur, 0, bdr, SCHEDULE_SPLIT_INDEX, 0, vec_only);
    test<T>("blur", build_blur, 0, bdr, SCHEDULE_SPLIT_INDEX, vec, vec_only);
# endif
    do_conv_n<T>(bdr, 0, 0, vec_only);
    do_conv_n<T>(bdr, SCHEDULE_ROOT_BORDER, 0, vec_only);
    do_conv_n<T>(bdr, 0, vec, vec_only);
    do_conv_n<T>(bdr, SCHEDULE_ROOT_BORDER, vec, vec_only);
# if HAS_PARTITION
    do_conv_n<T>(bdr, SCHEDULE_SPLIT_INDEX, 0, vec_only);
    do_conv_n<T>(bdr, SCHEDULE_SPLIT_INDEX | SCHEDULE_ROOT_BORDER, 0, vec_only);
    do_conv_n<T>(bdr, SCHEDULE_SPLIT_INDEX, vec, vec_only);
    do_conv_n<T>(bdr, SCHEDULE_SPLIT_INDEX | SCHEDULE_ROOT_BORDER, vec, vec_only);
# endif
    test<T>("simplify", build_simplify_n, 5, bdr, 0, 0, vec_only);
    test<T>("simplify", build_simplify_n, 10, bdr, 0, 0, vec_only);
    test<T>("simplify", build_simplify_n, 20, bdr, 0, 0, vec_only);
    test<T>("simplify", build_simplify_n, 40, bdr, 0, 0, vec_only);
    test<T>("simplify", build_simplify_n, 80, bdr, 0, 0, vec_only);
}


int main(int argc, char **argv) {
    // Command line information
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-log") == 0) logging = 1; // Compile and execute each function once only.
        else if (strcmp(argv[i], "-list") == 0) listing = 1;
        else if (strcmp(argv[i], "-t") == 0 && i < argc - 1) {
            // Parse time limit.
            sscanf(argv[i+1], "%d", &timelimit);
        } else if (strcmp(argv[i], "-m") == 0 && i < argc - 1) {
            model = argv[i+1];
        } else if (strcmp(argv[i], "-v") == 0 && i < argc - 1) {
            version = argv[i+1];
        } else {
# if 0
            // Parse version string.
            int version = 0;
            char typestring[21];
            int n;
            n = sscanf(argv[i], "%20[^_]_%d", typestring, &version);
            if (n == 2) {
                halide_version = version;
                halide_branch = typestring[0];
            }
# endif
            // Parse test ID number to be run.
            sscanf(argv[i], "%d", &testid);
        }
    }

    do_tests<uint8_t>(BORDER_CLAMP, 8);
    do_tests<uint8_t>(BORDER_MOD, 8);
# if 0
    do_tests<uint8_t>(BORDER_CLAMP, 16, 1);
    do_tests<uint8_t>(BORDER_MOD, 16, 1);
    do_tests<float>(BORDER_CLAMP, 4);
    do_tests<float>(BORDER_MOD, 4);
    do_tests<int>(BORDER_CLAMP, 4);
    do_tests<int>(BORDER_MOD, 4);
    do_tests<uint16_t>(BORDER_CLAMP, 8);
    do_tests<uint16_t>(BORDER_MOD, 8);
# endif

    // id -2 prints out a list of IDs that can be used in a loop to execute all tests in turn.
    // It does no tests.
    if (testid == -2) {
        for (int i = 0; i < testcount; i++) printf ("%d ", i);
        printf ("\n");
    }
    
    return 0;
}
