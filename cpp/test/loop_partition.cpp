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

// Image dimension in this test is 1280.  That is 5 * 256 so not a power of 2, but also
// a multiple of useful powers of 2.

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
    
    in(x,y) = input(clamp(x,0,1279), clamp(y,0,1279));
    
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
    
    Image<uint8_t> r1 = e1.realize(1280,1280);
    Image<uint8_t> r2 = e1.realize(1280,1280);
    
    // For some reason, the first call to realize
    // in this manner with a specified buffer is slow.
    // Perhaps it recompiles?
    
    double te1 = 1280000000.0;
    double te2 = 1280000000.0;
    
    int average = 200;
    int repeat = 2;
    int warmup = 10;
    
    for (int j = 0; j < repeat; j++) {
        for (int i = 0; i < warmup; i++) {
            e1.realize(r1);
        }
        double t1 = currentTime();
        for (int i = 0; i < average; i++) {
            e1.realize(r1);
        }
        double t2 = currentTime();
        
        for (int i = 0; i < warmup; i++) {
            e2.realize(r1);
        }
        double t3 = currentTime();
        for (int i = 0; i < average; i++) {
            e2.realize(r2);
        }
        double t4 = currentTime();
        //printf ("%g %g\n", t2 - t1, t3 - t2);
        //te1 = std::min(t2 - t1, te1);
        //te2 = std::min(t3 - t2, te2);
        te1 = t2 - t1;
        te2 = t4 - t3;
        printf ("%20s: normal: %7.3f partition: %7.3f\n", s.c_str(), te1/average, te2/average);
        
    }
    
}


main () {
    // Start by generating an input image. 1MP in size.
    Func init("init");
    Var x("x"), y("y"), yi("yi");
    int N = 1;
    init(x,y) = cast(UInt(8), (x + y * 123) % 256);
    Image<uint8_t> input = init.realize(1280,1280);
    
    // Now to apply edge detection
    Func e1("e1"), e2("e2");
    //Expr e = cast(UInt(8), sobel_amp(cast(Int(16), input), "x")(x,y));
    Expr e = input(clamp(x-1,0,1279),clamp(y-1,0,1279)) + input(clamp(x+1,0,1279),clamp(y+1,0,1279));
    e1(x,y) = e;
    e2(x,y) = e;
    e2.partition(x,N,N).partition(y,N,N);
    compare("simple no bound", e1, e2);
    
    Func e1b("e1b"), e2b("e2b");
    e1b(x,y) = e;
    e2b(x,y) = e;
    e1b.bound(x,0,1280).bound(y,0,1280);
    e2b.bound(x,0,1280).bound(y,0,1280).partition(x,N,N).partition(y,N,N);
    compare("simple", e1b, e2b);
    
    Func e3("e3"), e4("e4");
    e3(x,y) = e;
    e4(x,y) = e;
    e3.bound(x,0,1280).bound(y,0,1280).vectorize(x,8);
    e4.bound(x,0,1280).bound(y,0,1280).vectorize(x,8).partition(x,N,N).partition(y,N,N);
    compare("vector", e3, e4);
    
    Func norm_par("norm_par"), part_par("part_par");
    norm_par(x,y) = e;
    part_par(x,y) = e;
    norm_par.bound(x,0,1280).bound(y,0,1280).parallel(y);
    part_par.bound(x,0,1280).bound(y,0,1280).parallel(y).partition(x,N,N).partition(y,N,N);
    compare("parallel(1)", norm_par, part_par);
    
    Func norm_par4("norm_par4"), part_par4("part_par4");
    norm_par4(x,y) = e;
    part_par4(x,y) = e;
    norm_par4.bound(x,0,1280).bound(y,0,1280).split(y,y,yi,4).parallel(y);
    part_par4.bound(x,0,1280).bound(y,0,1280).split(y,y,yi,4).parallel(y).partition(x,N,N).partition(y,N,N);
    compare("parallel(4)", norm_par4, part_par4);
    
    Func norm_par8("norm_par8"), part_par8("part_par8");
    norm_par8(x,y) = e;
    part_par8(x,y) = e;
    norm_par8.bound(x,0,1280).bound(y,0,1280).split(y,y,yi,8).parallel(y);
    part_par8.bound(x,0,1280).bound(y,0,1280).split(y,y,yi,8).parallel(y).partition(x,N,N).partition(y,N,N);
    compare("parallel(8)", norm_par8, part_par8);
    
    Func norm_par16("norm_par16"), part_par16("part_par16");
    norm_par16(x,y) = e;
    part_par16(x,y) = e;
    norm_par16.bound(x,0,1280).bound(y,0,1280).split(y,y,yi,16).parallel(y);
    part_par16.bound(x,0,1280).bound(y,0,1280).split(y,y,yi,16).parallel(y).partition(x,N,N).partition(y,N,N);
    compare("parallel(16)", norm_par16, part_par16);
    
    Func norm_par_u8("norm_par_u8"), part_par_u8("part_par_u8");
    norm_par_u8(x,y) = e;
    part_par_u8(x,y) = e;
    norm_par_u8.bound(x,0,1280).bound(y,0,1280).parallel(y).unroll(x,8);
    part_par_u8.bound(x,0,1280).bound(y,0,1280).parallel(y).unroll(x,8).partition(x,N,N).partition(y,N,N);
    compare("par(1) unroll(8)", norm_par_u8, part_par_u8);
    
    Func norm_par4_u8("norm_par4_u8"), part_par4_u8("part_par4_u8");
    norm_par4_u8(x,y) = e;
    part_par4_u8(x,y) = e;
    norm_par4_u8.bound(x,0,1280).bound(y,0,1280).split(y,y,yi,4).parallel(y).unroll(x,8);
    part_par4_u8.bound(x,0,1280).bound(y,0,1280).split(y,y,yi,4).parallel(y).unroll(x,8).partition(x,N,N).partition(y,N,N);
    compare("par(4) unroll(8)", norm_par_u8, part_par_u8);
    
    Func party_par_u8("party_par_u8");
    party_par_u8(x,y) = e;
    party_par_u8.bound(x,0,1280).bound(y,0,1280).parallel(y).unroll(x,8).partition(y,N,N);
    compare("par(1) unroll(8) y", norm_par_u8, party_par_u8);
    
}