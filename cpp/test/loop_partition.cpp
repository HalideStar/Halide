#include <stdio.h>
#include <Halide.h>

using namespace Halide;

# define CHECK_EFFECTIVE 1


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
#define WIDTH 1280
#define HEIGHT 1280

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

Image<uint8_t> input;

int doclamp(int x, int lo, int hi) { return(x < lo ? lo : (x > hi ? hi : x)); }

void check_clamp(std::string s, Image<uint8_t> output, int k) {
    for (int i = 0; i < WIDTH; i++) {
        for (int j = 0; j < HEIGHT; j++) {
            uint8_t val;
            val = input(doclamp(i-k,0,WIDTH-1),doclamp(j-k,0,HEIGHT-1)) +
                  input(doclamp(i+k,0,WIDTH-1),doclamp(j+k,0,HEIGHT-1));
            if (val != output(i,j)) {
                printf ("Error: %s(%d,%d) is %d, expected %d\n", s.c_str(), i, j, output(i,j), val);
            }
        }
    }
}

// Border::wrap or mod
int dowrap(int x, int lo, int hi) { return((x - lo + (hi - lo + 1)) % (hi - lo + 1) + lo); }

void check_wrap(std::string s, Image<uint8_t> output, int k) {
    for (int i = 0; i < WIDTH; i++) {
        for (int j = 0; j < HEIGHT; j++) {
            uint8_t val;
            val = input(dowrap(i-k,0,WIDTH-1),dowrap(j-k,0,HEIGHT-1)) +
                  input(dowrap(i+k,0,WIDTH-1),dowrap(j+k,0,HEIGHT-1));
            if (val != output(i,j)) {
                printf ("Error: %s(%d,%d) is %d, expected %d\n", s.c_str(), i, j, output(i,j), val);
            }
        }
    }
}

int doconst(int c, int x, int xlo, int xhi, int y, int ylo, int yhi) { 
    if (x < xlo) return c; if (x > xhi) return c;  
    if (y < ylo) return c; if (y > yhi) return c;
    return input(x,y);
}

void check_const(std::string s, Image<uint8_t> output, int k) {
    for (int i = 0; i < WIDTH; i++) {
        for (int j = 0; j < HEIGHT; j++) {
            uint8_t val;
            val = doconst(1,i-k,0,WIDTH-1,j-k,0,HEIGHT-1) +
                  doconst(1,i+k,0,WIDTH-1,j+k,0,HEIGHT-1);
            if (val != output(i,j)) {
                printf ("Error: %s(%d,%d) is %d, expected %d\n", s.c_str(), i, j, output(i,j), val);
            }
        }
    }
}

void compare(std::string s, Func norm_nobound, Func part_nobound, 
             void (*check)(std::string, Image<uint8_t>, int), int k) {

# if CHECK_EFFECTIVE
    Internal::Stmt stmt = part_nobound.compile_to_stmt();
    if (! is_effective_partition(stmt)) {
        std::cerr << "Partitioning of " << part_nobound.name() << " was not fully effective\n";
    }
# endif
    
    Image<uint8_t> r1 = norm_nobound.realize(1280,1280);
    Image<uint8_t> r2 = part_nobound.realize(1280,1280);
    
    (*check)(s + " norm", r1, k);
    (*check)(s + " part", r2, k);
    
    // For some reason, the first call to realize
    // in this manner with a specified buffer is slow.
    // Perhaps it recompiles?
    
    double tnorm_nobound = 1280000000.0;
    double tpart_nobound = 1280000000.0;
    
    int average = 200;
    int repeat = 2;
    int warmup = 10;
    
    for (int j = 0; j < repeat; j++) {
        for (int i = 0; i < warmup; i++) {
            norm_nobound.realize(r1);
        }
        double t1 = currentTime();
        for (int i = 0; i < average; i++) {
            norm_nobound.realize(r1);
        }
        double t2 = currentTime();
        
        for (int i = 0; i < warmup; i++) {
            part_nobound.realize(r1);
        }
        double t3 = currentTime();
        for (int i = 0; i < average; i++) {
            part_nobound.realize(r2);
        }
        double t4 = currentTime();
        //printf ("%g %g\n", t2 - t1, t3 - t2);
        //tnorm_nobound = std::min(t2 - t1, tnorm_nobound);
        //tpart_nobound = std::min(t3 - t2, tpart_nobound);
        tnorm_nobound = t2 - t1;
        tpart_nobound = t4 - t3;
        printf ("%30s: norm: %7.3f   part: %7.3f %s\n", s.c_str(), tnorm_nobound/average, tpart_nobound/average,
            tpart_nobound > tnorm_nobound * 1.05 ? "*****" : "");
        
    }
    
}

void test (std::string prefix, std::string id, Expr e, int xlo, int xhi, int ylo, int yhi, 
           void (*check)(std::string, Image<uint8_t>, int), int k) {
    int N = 1;
    Var x("x"), y("y"), yi("yi");
    Interval xpart(xlo, xhi);
    Interval ypart(ylo, yhi);
    
    if (id == "_") {
        Func norm(prefix + ""), part(prefix + "_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm;
        part.partition();
        compare(prefix + "", norm, part, check, k);
    } else if (id == "_b") {
        Func norm(prefix + "_b"), part(prefix + "_b_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.bound(x,0,WIDTH).bound(y,0,HEIGHT);
        part.bound(x,0,WIDTH).bound(y,0,HEIGHT).partition();
        compare(prefix + "_b", norm, part, check, k);
    } else if (id == "_b_v8") {
        Func norm(prefix + "_b_v8"), part(prefix + "_b_v8_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.bound(x,0,WIDTH).bound(y,0,HEIGHT).vectorize(x,8);
        part.bound(x,0,WIDTH).bound(y,0,HEIGHT).vectorize(x,8).partition();
        compare(prefix + "_b_v8", norm, part, check, k);
    } else if (id == "_v8") {
        Func norm(prefix + "_v8"), part(prefix + "_v8_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.vectorize(x,8);
        part.vectorize(x,8).partition();
        compare(prefix + "_v8", norm, part, check, k);
    } else if (id == "_b_v8_m") {
        Func norm(prefix + "_b_v8_m"), part(prefix + "_b_v8_m_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.bound(x,0,WIDTH).bound(y,0,HEIGHT).vectorize(x,8);
        part.bound(x,0,WIDTH).bound(y,0,HEIGHT).partition(x,xpart).partition(y,true).vectorize(x,8);
        compare(prefix + "_b_v8_m", norm, part, check, k);
    } else if (id == "_v8_m") {
        // Manual partition without bound does not work because the partition is specified
        // in the output image dimension, which is unknown to the code generator.
        // On the other hand, automatic partition does the job perfectly well.
        Func norm(prefix + "_v8_m"), part(prefix + "_v8_m_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.vectorize(x,8);
        part.partition(x,xpart).partition(y,true).vectorize(x,8);
        compare(prefix + "_v8_m", norm, part, check, k);
    } else if (id == "_b_p1") {
        Func norm(prefix + "_b_p1"), part(prefix + "_b_p1_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.bound(x,0,WIDTH).bound(y,0,HEIGHT).parallel(y);
        part.bound(x,0,WIDTH).bound(y,0,HEIGHT).parallel(y).partition();
        compare(prefix + "_b_p1", norm, part, check, k);
    } else if (id == "_p1") {
        Func norm(prefix + "_p1"), part(prefix + "_p1_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.parallel(y);
        part.parallel(y).partition();
        compare(prefix + "_p1", norm, part, check, k);
    } else if (id == "_b_p4") {
        Func norm(prefix + "_b_p4"), part(prefix + "_b_p4_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.bound(x,0,WIDTH).bound(y,0,HEIGHT).split(y,y,yi,4).parallel(y);
        part.bound(x,0,WIDTH).bound(y,0,HEIGHT).split(y,y,yi,4).parallel(y).partition();
        compare(prefix + "_b_p4", norm, part, check, k);
    } else if (id == "_p4") {
        Func norm(prefix + "_p4"), part(prefix + "_p4_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.split(y,y,yi,4).parallel(y);
        part.split(y,y,yi,4).parallel(y).partition();
        compare(prefix + "_p4", norm, part, check, k);
    } else if (id == "_b_p8") {
        Func norm(prefix + "_b_p8"), part(prefix + "_b_p8_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.bound(x,0,WIDTH).bound(y,0,HEIGHT).split(y,y,yi,8).parallel(y);
        part.bound(x,0,WIDTH).bound(y,0,HEIGHT).split(y,y,yi,8).parallel(y).partition();
        compare(prefix + "_b_p8", norm, part, check, k);
    } else if (id == "_p8") {
        Func norm(prefix + "_p8"), part(prefix + "_p8_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.split(y,y,yi,8).parallel(y);
        part.split(y,y,yi,8).parallel(y).partition();
        compare(prefix + "_p8", norm, part, check, k);
    } else if (id == "_b_p16") {
        Func norm(prefix + "_b_p16"), part(prefix + "_b_p16_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.bound(x,0,WIDTH).bound(y,0,HEIGHT).split(y,y,yi,16).parallel(y);
        part.bound(x,0,WIDTH).bound(y,0,HEIGHT).split(y,y,yi,16).parallel(y).partition();
        compare(prefix + "_b_p16", norm, part, check, k);
    } else if (id == "_p16") {
        Func norm(prefix + "_p16"), part(prefix + "_p16_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.split(y,y,yi,16).parallel(y);
        part.split(y,y,yi,16).parallel(y).partition();
        compare(prefix + "_p16", norm, part, check, k);
    }  else if (id == "_b_p1_u2") {
        Func norm(prefix + "_b_p1_u2"), part(prefix + "_b_p1_u2_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.bound(x,0,WIDTH).bound(y,0,HEIGHT).parallel(y).unroll(x,2);
        part.bound(x,0,WIDTH).bound(y,0,HEIGHT).parallel(y).unroll(x,2).partition();
        compare(prefix + "_b_p1_u2", norm, part, check, k);
    }  else if (id == "_p1_u2") {
        Func norm(prefix + "_p1_u2"), part(prefix + "_p1_u2_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.parallel(y).unroll(x,2);
        part.parallel(y).unroll(x,2).partition();
        compare(prefix + "_p1_u2", norm, part, check, k);
    }  else if (id == "_b_p1_u4") {
        Func norm(prefix + "_b_p1_u4"), part(prefix + "_b_p1_u4_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.bound(x,0,WIDTH).bound(y,0,HEIGHT).parallel(y).unroll(x,4);
        part.bound(x,0,WIDTH).bound(y,0,HEIGHT).parallel(y).unroll(x,4).partition();
        compare(prefix + "_b_p1_u4", norm, part, check, k);
    }  else if (id == "_p1_u4") {
        Func norm(prefix + "_p1_u4"), part(prefix + "_p1_u4_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.parallel(y).unroll(x,4);
        part.parallel(y).unroll(x,4).partition();
        compare(prefix + "_p1_u4", norm, part, check, k);
    }  else if (id == "_b_p1_u8") {
        Func norm(prefix + "_b_p1_u8"), part(prefix + "_b_p1_u8_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.bound(x,0,WIDTH).bound(y,0,HEIGHT).parallel(y).unroll(x,8);
        part.bound(x,0,WIDTH).bound(y,0,HEIGHT).parallel(y).unroll(x,8).partition();
        compare(prefix + "_b_p1_u8", norm, part, check, k);
    }  else if (id == "_p1_u8") {
        Func norm(prefix + "_p1_u8"), part(prefix + "_p1_u8_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.parallel(y).unroll(x,8);
        part.parallel(y).unroll(x,8).partition();
        compare(prefix + "_p1_u8", norm, part, check, k);
    }  else if (id == "_b_p4_u8") {
        Func norm(prefix + "_b_p4_u8"), part(prefix + "_b_p4_u8_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.bound(x,0,WIDTH).bound(y,0,HEIGHT).split(y,y,yi,4).parallel(y).unroll(x,8);
        part.bound(x,0,WIDTH).bound(y,0,HEIGHT).split(y,y,yi,4).parallel(y).unroll(x,8).partition();
        compare(prefix + "_b_p4_u8", norm, part, check, k);
    }  else if (id == "_p4_u8") {
        Func norm(prefix + "_p4_u8"), part(prefix + "_p4_u8_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.split(y,y,yi,4).parallel(y).unroll(x,8);
        part.split(y,y,yi,4).parallel(y).unroll(x,8).partition();
        compare(prefix + "_p4_u8", norm, part, check, k);
    }
}


# define CODEVIEW_1D 0
# define CODEVIEW_2D 0
# define SPEED_CLAMP 0
# define SPEED_CLAMP_BDR 0
# define SPEED_MOD 0
# define SPEED_SELECT 1
# define SPEED_SELECT_CLAMP 1
# define SPEED_WRAP 0

std::string test_idlist_all[] = { "_", "_b", "_v8", /* "_v8_m", */ "_b_v8", "_b_v8_m", 
    "_p1", "_b_p1", "_p4", "_b_p4", "_p8", "_b_p8", "_p16", "_b_p16", 
    "_p1_u2", "_b_p1_u2", "_p1_u4", "_b_p1_u4", "_p1_u8", "_b_p1_u8", "_p4_u8", "_b_p4_u8", "" };

# define IDLIST test_idlist_all

main () {
    // Start by generating an input image. 1MP in size.
    Func init("init");
    Var x("x"), y("y"), yi("yi");
    init(x,y) = cast(UInt(8), (x + y * 123) % 256);
    input = init.realize(WIDTH,HEIGHT);
    
    Func init1d("init1d");
    init1d(x) = cast(UInt(8), (x * 23) % 256);
    
    Image<uint8_t> input1d;
    input1d = init1d.realize(WIDTH);
    
    Func undefined;
    
    Halide::global_options.loop_partition = true;
    std::cout << global_options;

    const int max_diag = 1;

# if CODEVIEW_1D
    const int the_diag = 1;
    // 1-D cases for code viewing and analysis.
    std::cout << global_options;
    for (int k = the_diag; k <= the_diag; k++) {
        std::ostringstream ss;
        ss << "f1d_" << k;
        Expr e = input1d(clamp(x-k,0,WIDTH-1)) + input1d(clamp(x+k,0,WIDTH-1));
        Func s_v8(ss.str() + "_s_v8");
        s_v8(x) = e;
        s_v8.vectorize(x,8).partition();
        s_v8.compile_jit();
        Func b_s_v8(ss.str() + "_b_s_v8");
        b_s_v8(x) = e;
        b_s_v8.bound(x,0,WIDTH-1).vectorize(x,8).partition();
        //b_s_v8.compile_jit();
    }
# endif
    
# if CODEVIEW_2D
    const int the_diag = 1;
    // 2-D cases for code viewing and analysis.
    std::cout << global_options;
    for (int k = the_diag; k <= the_diag; k++) {
        std::ostringstream ss;
        ss << "f2d_" << k;
        Expr e = input(clamp(x-k,0,WIDTH-1),clamp(y-k,0,WIDTH-1)) + input(clamp(x+k,0,WIDTH-1),clamp(y+k,0,WIDTH-1));
        Func s_p4_u8(ss.str() + "_s_p4_u8");
        s_p4_u8(x,y) = e;
        s_p4_u8.split(y,y,yi,4).parallel(y).unroll(x,8).partition(x, true).partition(y,true);
        s_p4_u8.compile_jit();
    }
# endif
    
    for (int j = 0; IDLIST[j] != ""; j++) {
    for (int k = 1; k <= max_diag; k++) {
# if SPEED_CLAMP
    // 2-D simple diagonal case: good for speed testing.
    {
        std::ostringstream ss;
        ss << "clamp_" << k;
        Expr e = input(clamp(x-k,0,WIDTH-1),clamp(y-k,0,HEIGHT-1)) + input(clamp(x+k,0,WIDTH-1),clamp(y+k,0,HEIGHT-1));
        test(ss.str(),IDLIST[j],e,k,WIDTH-1-k,k,HEIGHT-1-k, check_clamp, k);
    }
# endif

# if SPEED_CLAMP_BDR
    // 2-D simple diagonal case: good for speed testing.
    {
        std::ostringstream ss;
        ss << "clamp_bdr_" << k;
        Func b;
        b(x,y) = input(clamp(x,0,WIDTH-1),clamp(y,0,WIDTH-1));
        Expr e = b(x-k,y-k) + b(x+k,y+k);
        test(ss.str(),IDLIST[j],e,k,WIDTH-1-k,k,HEIGHT-1-k, check_clamp, k);
    }
# endif

# if SPEED_MOD
    // 2-D simple diagonal case: good for speed testing.
    {
        std::ostringstream ss;
        ss << "mod_" << k;
        Func b;
        b(x,y) = input(x%WIDTH,y%HEIGHT);
        Expr e = b(x-k,y-k) + b(x+k,y+k);
        test(ss.str(),IDLIST[j],e,k,WIDTH-1-k,k,HEIGHT-1-k, check_wrap, k);
    }
# endif

# if SPEED_SELECT
    // 2-D simple diagonal case: good for speed testing.
    {
        std::ostringstream ss;
        ss << "select_mod_" << k;
        Func b;
        int c = 1;
        b(x,y) = select(x<0,c,select(x>WIDTH-1,c,
                 select(y<0,c,select(y>HEIGHT-1,c,input(x%WIDTH,y%HEIGHT)))));
        Expr e = b(x-k,y-k) + b(x+k,y+k);
        test(ss.str(),IDLIST[j],e,k,WIDTH-1-k,k,HEIGHT-1-k, check_const, k);
    }
# endif

# if SPEED_SELECT_CLAMP
    // 2-D simple diagonal case: good for speed testing.
    {
        std::ostringstream ss;
        ss << "select_clamp_" << k;
        Func b;
        int c = 1;
        b(x,y) = select(x<0,c,select(x>WIDTH-1,c,
                 select(y<0,c,select(y>HEIGHT-1,c,input(clamp(x,0,WIDTH-1),clamp(y,0,WIDTH-1))))));
        Expr e = b(x-k,y-k) + b(x+k,y+k);
        test(ss.str(),IDLIST[j],e,k,WIDTH-1-k,k,HEIGHT-1-k, check_const, k);
    }
# endif

# if SPEED_WRAP
    // 2-D simple diagonal case: good for speed testing.
    {
        std::ostringstream ss;
        ss << "diag_" << k;
        Func b = Border::wrap(input);
        Expr e = b(x-k,y-k) + b(x+k,y+k);
        test(ss.str(),IDLIST[j],e,k,WIDTH-1-k,k,HEIGHT-1-k, check_wrap, k);
    }
# endif
    }
    }

#if 0
    Func cons = Border::constant(0)(input);
    Expr part_nobound = cons(x-1,y-1) + cons(x+1,y+1);
    test("cons0", part_nobound,1,WIDTH-2,1,HEIGHT-2,undefined);
    
    Func blurx("blurx");
    blurx(x,y) = input(clamp(x-1,0,WIDTH-1),y) + input(x,y) + input(clamp(x+1,0,WIDTH-1),y);
    Expr blury = blurx(x,clamp(y-1,0,HEIGHT-1)) + blurx(x,y) + blurx(x,clamp(y+1,0,HEIGHT-1));
    test("blur", blury,1,WIDTH-2,1,HEIGHT-2,blurx);
#endif
}