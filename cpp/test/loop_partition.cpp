#include <stdio.h>
#include <Halide.h>

using namespace Halide;


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

// This is an implementation of sobel_amp based directly on the documentation of MVTec HALCON
// see http://www.mvtec.com/download/reference/sobel_amp.html .  
// That documentation defines the filters to be applied as convolution matrices; we have written the
// same filters directly.  We have implemented the filtertype parameter but not the size parameter.
// The size parameter represents a smoothing filter applied before sobel processing commences, and
// it is more general to leave this smoothing to the application programmer.
Func sobel_amp (Func input, std::string filtertype)
{
    Func in("in"), b("sobel_horiz"), a("sobel_vert"), amp("sobel_amp_" + filtertype);
    Var x("x"), y("y");
    
    in(x,y) = input(clamp(x,0,999), clamp(y,0,999));
    
    // Direct implementation of horizontal and vertical derivative
    // x is horizontal direction, y is vertical (down) direction
    // It may be possible to achieve a slight improvement by exploiting commonality of the diagonal differences
    // Following MVTec HALCON's definition of sobel_amp.  A is vert and B is horiz.
    b(x,y) = in(x-1,y-1) + 2 * in(x-1,y) + in(x-1,y+1) - in(x+1,y-1) - 2 * in(x+1,y) - in(x+1,y+1);
    a(x,y) = -in(x-1,y-1) - 2 * in(x,y-1) - in(x+1,y-1) + in(x-1,y+1) + 2 * in(x,y+1) + in(x+1,y+1);
    
    if (filtertype == "sum_sqrt")
        amp(x,y) = clamp(sqrt(a(x,y)*a(x,y) + b(x,y)*b(x,y)) / 4, 0, 65535); // For 16-bit input images.
    else if (filtertype == "sum_abs")
        amp(x,y) = (abs(a(x,y)) + abs(b(x,y))) / 4;
    else if (filtertype == "x")
        amp(x,y) = b(x,y) / 4;
    else if (filtertype == "y")
        amp(x,y) = a(x,y) / 4;
    else if (filtertype == "thin_sum_abs" || filtertype == "thin_max_abs")
    {
        Func absb("sobel_horiz_abs"), absa("sobel_vert_abs"), thinb("sobel_horiz_thin"), thina("sobel_vert_thin");
        
        absa = abs(a);
        absb = abs(b);
        // Vertical gradient is thinned vertically; horizontal gradient is thinned horizontally
        thina(x,y) = select(absa(x,y) >= absa(x,y-1) && absa(x,y) >= absa(x,y+1), absa(x,y), 0);
        thinb(x,y) = select(absb(x,y) >= absb(x-1,y) && absb(x,y) >= absb(x+1,y), absb(x,y), 0);
        
        if (filtertype == "thin_sum_abs")
            amp(x,y) = (thina(x,y) + thinb(x,y)) / 4;
        else
            amp(x,y) = max(thina(x,y), thinb(x,y)) / 4;
    }
    
    return amp;
}

void compare(std::string s, Func e1, Func e2) {
    
    Image<uint8_t> r1 = e1.realize(1000,1000);
    Image<uint8_t> r2 = e1.realize(1000,1000);
    
    // For some reason, the first call to realize
    // in this manner with a specified buffer is slow.
    // Perhaps it recompiles?
    e1.realize(r1);
    e2.realize(r2);
    
    double te1 = 1000000000.0;
    double te2 = 1000000000.0;
    
    for (int j = 0; j < 5; j++) {
        double t1 = currentTime();
        for (int i = 0; i < 10; i++) {
            e1.realize(r1);
        }
        double t2 = currentTime();
        for (int i = 0; i < 10; i++) {
            e2.realize(r2);
        }
        double t3 = currentTime();
        //printf ("%g %g\n", t2 - t1, t3 - t2);
        te1 = std::min(t2 - t1, te1);
        te2 = std::min(t3 - t2, te2);
        
    }
    
    printf ("%s: normal: %g partition: %g\n", s.c_str(), te1/10.0, te2/10.0);
}


main () {
    // Start by generating an input image. 1MP in size.
    Func init("init");
    Var x("x"), y("y"), yi("yi");
    int N = 2;
    init(x,y) = cast(UInt(8), (x + y * 123) % 256);
    Image<uint8_t> input = init.realize(1000,1000);
    
    // Now to apply edge detection
    Func e1("e1"), e2("e2");
    Expr e = cast(UInt(8), sobel_amp(cast(Int(16), input), "x")(x,y));
    e1(x,y) = e;
    e2(x,y) = e;
    e2.bound(x,0,1000).bound(y,0,1000).partition(x,N,N).partition(y,N,N);
    compare("simple", e1, e2);
    
    Func e3("e3"), e4("e4");
    e3(x,y) = e;
    e4(x,y) = e;
    e3.vectorize(x,8);
    e4.bound(x,0,1000).bound(y,0,1000).vectorize(x,8).partition(x,N,N).partition(y,N,N);
    compare("vector", e3, e4);
    
    Func e5("e5"), e6("e6");
    e5(x,y) = e;
    e6(x,y) = e;
    e5.parallel(y);
    e6.bound(x,0,1000).bound(y,0,1000).parallel(y).partition(x,N,N).partition(y,N,N);
    compare("parallel", e5, e6);
    
    Func e7("e7"), e8("e8");
    e7(x,y) = e;
    e8(x,y) = e;
    e7.split(y,y,yi,8).parallel(y);
    e8.bound(x,0,1000).bound(y,0,1000).split(y,y,yi,8).parallel(y).partition(x,N,N).partition(y,N,N);
    compare("parallel block", e7, e8);
    
}