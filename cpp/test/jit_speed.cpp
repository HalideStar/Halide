#include <stdio.h>
#include <Halide.h>
#include <string.h>
#include <signal.h>

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

# define WIDTH 1000
# define HEIGHT 1000

# define SCHEDULE_PARTITION 1

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

// Test ID passed as parameter; -1 means run all, but a timeout will abandon subsequent tests.
int testid = -1;

// Set with -t option to limit time allowed for each individual compilation, in seconds.
int timelimit = 10;
std::string the_name;

void alarm_handler(int sig) {
    signal(SIGALRM, SIG_IGN);
    fprintf (stderr, "%s: Timeout\n", the_name.c_str());
    printf("%s,%s,%s,%.1f,%.1f,%.1f,0,0\n", model.c_str(), version.c_str(), the_name.c_str(), -1.0, -1.0,-1.0);
    exit(0);
}



Func build_basic(ImageParam a, Image<int> b, int n, int schedule) {
    Var x("x");
    Func f("basic");
    f(x) = a(x) + b(x);
    return f;
}

Func build_simplify_n(ImageParam a, Image<int> b, int n, int schedule) {
    Var x("x");
    char str[1000];
    sprintf (str, "simplify_%d", n);
    Func f(str);
    Expr e = a(x) * 65;
    for (int i = 1; i < n; i++)
        e = e + a(x) * (i + 65);
    f(x) = e;
    return f;
}

Func build_conv_n(ImageParam a, Image<int> b, int n, int schedule) {
    Var x("x");
    char str[1000];
    sprintf (str, "conv_%d%s", n, (schedule & SCHEDULE_PARTITION) ? "_p" : "");
    Func f(str);
    Func in("input");
    in(x) = a(clamp(x, 0, WIDTH-1));
    Expr e = in(x-n/2) * 65;
    for (int i = 1; i < n; i++)
        e = e + in(x+(i-n/2)) * (i + 65);
    f(x) = e;
# if HAS_PARTITION
    if (schedule & SCHEDULE_PARTITION) f.partition();
# endif
    return f;
}

// Counts the calls to test, so each test gets a unique ID.
int testcount = 0;

void test(std::string name, Func (*builder)(ImageParam, Image<int>, int, int), int n = 0, int schedule = 0) {
    if (testcount++ != testid && testid != -1) return; // Skip this test.
    
    ImageParam a(Int(32), 2);
    Image<int> b(WIDTH,HEIGHT), c(WIDTH,HEIGHT), d(WIDTH,HEIGHT);
    a.set(c);

    double t1, t2;
    t1 = currentTime();

    int count, check = 1;
    the_name = name;
    alarm(timelimit);
    signal(SIGALRM, alarm_handler);
    for (count = 0; count < (logging ? 1 : 100); count++) {
        (*builder)(a, b, n, schedule).compile_jit();
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
    t1 = currentTime();
    check = 1;
    for (count = 0; count < (logging ? 1 : 100); count++) {
        (*builder)(a, b, n, schedule).compile_to_stmt();
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
# endif

    fprintf (stderr, "%s: %.1f ms per lower\n", name.c_str(), stmt);
    
    

    // Now to estimate execution time.
    Func f = (*builder)(a, b, n, schedule);
    f.realize(d);
    
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
    printf ("%s,%s,%s,%.1f,%.1f,%.1f,%d,%d\n", model.c_str(), version.c_str(), name.c_str(), jit, stmt, execute, hits, misses);
    fflush(stdout);
}

int main(int argc, char **argv) {
    // Command line information
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-log") == 0) logging = 1; // Compile and execute each function once only.
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
    test("basic", build_basic, 0);
    test("conv_10", build_conv_n, 10);
    test("conv_20", build_conv_n, 20);
    test("conv_40", build_conv_n, 40);
    test("conv_80", build_conv_n, 80);
# if HAS_PARTITION
    test("conv_10_p", build_conv_n, 10, SCHEDULE_PARTITION);
    test("conv_20_p", build_conv_n, 20, SCHEDULE_PARTITION);
    test("conv_40_p", build_conv_n, 40, SCHEDULE_PARTITION);
    test("conv_80_p", build_conv_n, 80, SCHEDULE_PARTITION);
# endif
    test("simplify_10", build_simplify_n, 10);
    test("simplify_100", build_simplify_n, 100);
    
    // id -2 prints out a list of IDs that can be used in a loop to execute all tests in turn.
    if (testid == -2) {
        for (int i = 0; i < testcount; i++) printf ("%d ", i);
        printf ("\n");
    }
    
    return 0;
}
