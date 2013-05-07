#include <stdio.h>
#include <Halide.h>

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

Func build_basic(ImageParam a, Image<int> b, int n = 0, int schedule = 0) {
    Var x("x");
    Func f("basic");
    f(x) = a(x) + b(x);
    return f;
}

Func build_simplify_n(ImageParam a, Image<int> b, int n, int schedule = 0) {
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

Func build_conv_n(ImageParam a, Image<int> b, int n, int schedule = 0) {
    Var x("x");
    char str[1000];
    sprintf (str, "conv_%d", n);
    Func f(str);
    Func in("input");
    in(x) = a(clamp(x, 0, WIDTH-1));
    Expr e = in(x-n/2) * 65;
    for (int i = 1; i < n; i++)
        e = e + in(x+(i-n/2)) * (i + 65);
    f(x) = e;
    if (schedule & SCHEDULE_PARTITION) f.partition();
    return f;
}

void test(std::string name, Func (*builder)(ImageParam, Image<int>, int, int), int n = 0, int schedule = 0) {
    ImageParam a(Int(32), 2);
    Image<int> b(WIDTH,HEIGHT), c(WIDTH,HEIGHT), d(WIDTH,HEIGHT);
    a.set(c);

    double t1, t2;
    t1 = currentTime();

    int count, check = 8;
    for (count = 0; count < 100; count++) {
        (*builder)(a, b, n, schedule).compile_jit();
        if (count >= check) {
            t2 = currentTime();
            if (t2 - t1 > 2000.0)
                break;
            check = check * 2;
        }
    }    

    t2 = currentTime();
    double jit = (t2 - t1) / count;
    
    fprintf (stderr, "%s: %.1f ms per jit\n", name.c_str(), jit);

    // Now to estimate execution time.
    Func f = (*builder)(a, b, n, schedule);
    f.realize(d);
    
    t1 = currentTime();
    check = 4;
    for (count = 0; count < 1000; count++) {
        f.realize(d);
        if (count >= check) {
            t2 = currentTime();
            if (t2 - t1 > 2000.0)
                break;
            check = check * 2;
        }
    }
    
    double execute = (t2 - t1) / count;
    
    fprintf (stderr, "%s: %.1f ms per execution\n", name.c_str(), execute);
    printf ("%s: %.1f ms per jit, %.1f ms per execution\n", name.c_str(), jit, execute);
}

int main(int argc, char **argv) {
    //test("basic", build_basic, 0);
    test("conv_10", build_conv_n, 10);
    test("conv_20", build_conv_n, 20);
    test("conv_40", build_conv_n, 40);
    test("conv_80", build_conv_n, 80);
    test("conv_10_p", build_conv_n, 10, SCHEDULE_PARTITION);
    test("conv_20_p", build_conv_n, 20, SCHEDULE_PARTITION);
    test("conv_40_p", build_conv_n, 40, SCHEDULE_PARTITION);
    test("conv_80_p", build_conv_n, 80, SCHEDULE_PARTITION);
    test("simplify_10", build_simplify_n, 10);
    test("simplify_100", build_simplify_n, 100);
    
    return 0;
}
