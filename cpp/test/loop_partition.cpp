#include <stdio.h>
#include <Halide.h>

using namespace Halide;

# define CHECK_EFFECTIVE 1

int auto_test = 0;

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
                if (auto_test) assert(0);
            }
        }
    }
}

void check_clamp_2(std::string s, Image<uint8_t> output, int k) {
    for (int i = 0; i < WIDTH; i++) {
        for (int j = 0; j < HEIGHT; j++) {
            uint8_t val, v1, v2;
            int i1, i2, j1, j2;
            // The second stage clamps the indices, so we compute
            // i1,j1  i2,j2  as the indices that the second stage
            // will use to access the first stage.
            i1 = doclamp(i-k, 0, WIDTH-1);
            i2 = doclamp(i+k, 0, WIDTH-1);
            j1 = doclamp(j-k, 0, HEIGHT-1);
            j2 = doclamp(j+k, 0, HEIGHT-1);
            v1 = input(doclamp(i1-k,0,WIDTH-1),doclamp(j1-k,0,HEIGHT-1)) +
                  input(doclamp(i1+k,0,WIDTH-1),doclamp(j1+k,0,HEIGHT-1));
            v2 = input(doclamp(i2-k,0,WIDTH-1),doclamp(j2-k, 0,HEIGHT-1)) +
                  input(doclamp(i2+k,0,WIDTH-1),doclamp(j2+k,0,HEIGHT-1));
            val = v1 + v2;
            if (val != output(i,j)) {
                printf ("Error: %s(%d,%d) is %d, expected %d\n", s.c_str(), i, j, output(i,j), val);
                if (auto_test) assert(0);
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
                if (auto_test) assert(0);
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
                if (auto_test) assert(0);
            }
        }
    }
}

double fastest_time = 99999999.0, slowest_time = 0.0;
std::string fastest_id, slowest_id;

void compare(std::string s, Func norm_nobound, Func part_nobound, 
             void (*check)(std::string, Image<uint8_t>, int), int k) {

    if (CHECK_EFFECTIVE || auto_test) {
        Internal::Stmt stmt = part_nobound.compile_to_stmt();
        if (! is_effective_loop_split(stmt)) {
            std::cerr << "Loop splitting of " << part_nobound.name() << " was not fully effective\n";
            if (auto_test) assert(0);
        }
    }
    
    Image<uint8_t> r1 = norm_nobound.realize(1280,1280);
    Image<uint8_t> r2 = part_nobound.realize(1280,1280);
    
    (*check)(s + " norm", r1, k);
    (*check)(s + " part", r2, k);
    
    double tnorm_nobound = 99999999.0;
    double tpart_nobound = 99999999.0;
    
    int average = 20;
    int repeat = 5;
    int warmup = 2;
    int nfail = 0;
    const double mintime = 0.20; // 200ms
    
    double total_norm = 0.0, total_part = 0.0;
    int tot_done_norm = 0, tot_done_part = 0;
    for (int j = 0; j < repeat; j++) {
        for (int i = 0; i < warmup; i++) {
            norm_nobound.realize(r1);
        }
        average = 20;
        double t2 = currentTime();
        double t1 = currentTime();
        int done_norm = 0;
        while (t2 - t1 < mintime) {
            for (int i = 0; i < average; i++) {
                norm_nobound.realize(r1);
            }
            t2 = currentTime();
            done_norm += average;
            average = average * 2;
        }
        
        for (int i = 0; i < warmup; i++) {
            part_nobound.realize(r1);
        }
        average = 20;
        double t4 = currentTime();
        double t3 = currentTime();
        int done_part = 0;
        while (t4 - t3 < mintime) {
            for (int i = 0; i < average; i++) {
                part_nobound.realize(r2);
            }
            t4 = currentTime();
            done_part += average;
            average = average * 2;
        }
        //printf ("%g %g\n", t2 - t1, t3 - t2);
        //tnorm_nobound = std::min(t2 - t1, tnorm_nobound);
        //tpart_nobound = std::min(t3 - t2, tpart_nobound);
        tnorm_nobound = (t2 - t1) / done_norm;
        tpart_nobound = (t4 - t3) / done_part;
        total_norm += (t2 - t1);
        tot_done_norm += done_norm;
        total_part += (t4 - t3);
        tot_done_part += done_part;
        if (! auto_test) {
            printf ("%30s: norm: %7.3f   part: %7.3f %s\n", s.c_str(), tnorm_nobound, tpart_nobound,
                tpart_nobound > tnorm_nobound ? "*****" : "");
        }
        if (tpart_nobound > tnorm_nobound) nfail++;
    }
    double time_norm, time_part;
    time_norm = total_norm / tot_done_norm;
    time_part = total_part / tot_done_part;
    if (time_part < fastest_time) {
        fastest_time = time_part;
        fastest_id = s + " split";
    }
    if (time_part > slowest_time) {
        slowest_time = time_part;
        slowest_id = s + " split";
    }
    if (time_norm < fastest_time) {
        fastest_time = time_norm;
        fastest_id = s + " norm";
    }
    if (time_norm > slowest_time) {
        slowest_time = time_norm;
        slowest_id = s + " norm";
    }
    if (auto_test) {
        printf ("%20s: speed-up %.2f times   %.3f %.3f\n", s.c_str(), 
                time_norm / time_part, time_norm, time_part);
    }
    
    if (nfail >= repeat) {
        std::cerr << "Loop splitting did not achieve improved performance\n";
        if (auto_test) assert(0);
    }
}

void test (std::string prefix, std::string id, Expr e, int xlo, int xhi, int ylo, int yhi, 
           void (*check)(std::string, Image<uint8_t>, int), int k, int depth = 1) {
    int N = 1;
    Var x("x"), y("y"), yi("yi");
    Interval xpart(xlo, xhi);
    Interval ypart(ylo, yhi);
    
    if (id == "_") {
        Func norm(prefix + ""), part(prefix + "_s");
        Func alls(prefix + "_as");
        norm(x,y) = e;
        part(x,y) = e;
        alls(x,y) = e;
        part.loop_split();
        alls.loop_split_all();
        compare(prefix + "", norm, part, check, k);
        if (depth > 1) compare(prefix + "_as", part, alls, check, k);
    } else if (id == "_b") {
        Func norm(prefix + "_b"), part(prefix + "_b_s");
        Func alls(prefix + "_b_as");
        norm(x,y) = e;
        part(x,y) = e;
        alls(x,y) = e;
        norm.bound(x,0,WIDTH).bound(y,0,HEIGHT);
        part.bound(x,0,WIDTH).bound(y,0,HEIGHT).loop_split();
        alls.bound(x,0,WIDTH).bound(y,0,HEIGHT).loop_split_all();
        compare(prefix + "_b", norm, part, check, k);
        if (depth > 1) compare(prefix + "_b_as", part, alls, check, k);
    } else if (id == "_b_v8") {
        Func norm(prefix + "_b_v8"), part(prefix + "_b_v8_s");
        Func alls(prefix + "_b_v8_as");
        norm(x,y) = e;
        part(x,y) = e;
        alls(x,y) = e;
        norm.bound(x,0,WIDTH).bound(y,0,HEIGHT).vectorize(x,8);
        part.bound(x,0,WIDTH).bound(y,0,HEIGHT).vectorize(x,8).loop_split();
        alls.bound(x,0,WIDTH).bound(y,0,HEIGHT).vectorize(x,8).loop_split_all();
        compare(prefix + "_b_v8", norm, part, check, k);
        if (depth > 1) compare(prefix + "_b_v8_as", part, alls, check, k);
    } else if (id == "_v8") {
        Func norm(prefix + "_v8"), part(prefix + "_v8_s");
        Func alls(prefix + "_v8_as");
        norm(x,y) = e;
        part(x,y) = e;
        alls(x,y) = e;
        norm.vectorize(x,8);
        part.vectorize(x,8).loop_split();
        alls.vectorize(x,8).loop_split_all();
        compare(prefix + "_v8", norm, part, check, k);
        if (depth > 1) compare(prefix + "_v8_as", part, alls, check, k);
    } else if (id == "_b_v8_m") {
        Func norm(prefix + "_b_v8_m"), part(prefix + "_b_v8_m_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.bound(x,0,WIDTH).bound(y,0,HEIGHT).vectorize(x,8);
        part.bound(x,0,WIDTH).bound(y,0,HEIGHT).loop_split(x,xpart).loop_split(y,true).vectorize(x,8);
        compare(prefix + "_b_v8_m", norm, part, check, k);
    } else if (id == "_v8_m") {
        // Manual loop_split without bound does not work because the loop_split is specified
        // in the output image dimension, which is unknown to the code generator.
        // On the other hand, automatic loop_split does the job perfectly well.
        Func norm(prefix + "_v8_m"), part(prefix + "_v8_m_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.vectorize(x,8);
        part.loop_split(x,xpart).loop_split(y,true).vectorize(x,8);
        compare(prefix + "_v8_m", norm, part, check, k);
    } else if (id == "_b_p1") {
        Func norm(prefix + "_b_p1"), part(prefix + "_b_p1_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.bound(x,0,WIDTH).bound(y,0,HEIGHT).parallel(y);
        part.bound(x,0,WIDTH).bound(y,0,HEIGHT).parallel(y).loop_split();
        compare(prefix + "_b_p1", norm, part, check, k);
    } else if (id == "_p1") {
        Func norm(prefix + "_p1"), part(prefix + "_p1_s");
        Func alls(prefix + "_p1_as");
        norm(x,y) = e;
        part(x,y) = e;
        alls(x,y) = e;
        norm.parallel(y);
        part.parallel(y).loop_split();
        alls.parallel(y).loop_split_all();
        compare(prefix + "_p1", norm, part, check, k);
        if (depth > 1) compare(prefix + "_p1_as", part, alls, check, k);
    } else if (id == "_p1_v8") {
        Func norm(prefix + "_p1_v8"), part(prefix + "_p1__v8s");
        Func alls(prefix + "_p1_v8_as");
        norm(x,y) = e;
        part(x,y) = e;
        alls(x,y) = e;
        norm.vectorize(x,8).parallel(y);
        part.vectorize(x,8).parallel(y).loop_split();
        alls.vectorize(x,8).parallel(y).loop_split_all();
        compare(prefix + "_p1_v8", norm, part, check, k);
        if (depth > 1) compare(prefix + "_p1_v8_as", part, alls, check, k);
    } else if (id == "_b_p4") {
        Func norm(prefix + "_b_p4"), part(prefix + "_b_p4_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.bound(x,0,WIDTH).bound(y,0,HEIGHT).split(y,y,yi,4).parallel(y);
        part.bound(x,0,WIDTH).bound(y,0,HEIGHT).split(y,y,yi,4).parallel(y).loop_split();
        compare(prefix + "_b_p4", norm, part, check, k);
    } else if (id == "_p4") {
        Func norm(prefix + "_p4"), part(prefix + "_p4_s");
        Func alls(prefix + "_p4_as");
        norm(x,y) = e;
        part(x,y) = e;
        alls(x,y) = e;
        norm.split(y,y,yi,4).parallel(y);
        part.split(y,y,yi,4).parallel(y).loop_split();
        alls.split(y,y,yi,4).parallel(y).loop_split_all();
        compare(prefix + "_p4", norm, part, check, k);
        if (depth > 1) compare(prefix + "_p4_as", part, alls, check, k);
    } else if (id == "_b_p8") {
        Func norm(prefix + "_b_p8"), part(prefix + "_b_p8_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.bound(x,0,WIDTH).bound(y,0,HEIGHT).split(y,y,yi,8).parallel(y);
        part.bound(x,0,WIDTH).bound(y,0,HEIGHT).split(y,y,yi,8).parallel(y).loop_split();
        compare(prefix + "_b_p8", norm, part, check, k);
    } else if (id == "_p8") {
        Func norm(prefix + "_p8"), part(prefix + "_p8_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.split(y,y,yi,8).parallel(y);
        part.split(y,y,yi,8).parallel(y).loop_split();
        compare(prefix + "_p8", norm, part, check, k);
    } else if (id == "_b_p16") {
        Func norm(prefix + "_b_p16"), part(prefix + "_b_p16_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.bound(x,0,WIDTH).bound(y,0,HEIGHT).split(y,y,yi,16).parallel(y);
        part.bound(x,0,WIDTH).bound(y,0,HEIGHT).split(y,y,yi,16).parallel(y).loop_split();
        compare(prefix + "_b_p16", norm, part, check, k);
    } else if (id == "_p16") {
        Func norm(prefix + "_p16"), part(prefix + "_p16_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.split(y,y,yi,16).parallel(y);
        part.split(y,y,yi,16).parallel(y).loop_split();
        compare(prefix + "_p16", norm, part, check, k);
    }  else if (id == "_b_p1_u2") {
        Func norm(prefix + "_b_p1_u2"), part(prefix + "_b_p1_u2_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.bound(x,0,WIDTH).bound(y,0,HEIGHT).parallel(y).unroll(x,2);
        part.bound(x,0,WIDTH).bound(y,0,HEIGHT).parallel(y).unroll(x,2).loop_split();
        compare(prefix + "_b_p1_u2", norm, part, check, k);
    }  else if (id == "_p1_u2") {
        Func norm(prefix + "_p1_u2"), part(prefix + "_p1_u2_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.parallel(y).unroll(x,2);
        part.parallel(y).unroll(x,2).loop_split();
        compare(prefix + "_p1_u2", norm, part, check, k);
    }  else if (id == "_b_p1_u4") {
        Func norm(prefix + "_b_p1_u4"), part(prefix + "_b_p1_u4_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.bound(x,0,WIDTH).bound(y,0,HEIGHT).parallel(y).unroll(x,4);
        part.bound(x,0,WIDTH).bound(y,0,HEIGHT).parallel(y).unroll(x,4).loop_split();
        compare(prefix + "_b_p1_u4", norm, part, check, k);
    }  else if (id == "_p1_u4") {
        Func norm(prefix + "_p1_u4"), part(prefix + "_p1_u4_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.parallel(y).unroll(x,4);
        part.parallel(y).unroll(x,4).loop_split();
        compare(prefix + "_p1_u4", norm, part, check, k);
    }  else if (id == "_b_p1_u8") {
        Func norm(prefix + "_b_p1_u8"), part(prefix + "_b_p1_u8_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.bound(x,0,WIDTH).bound(y,0,HEIGHT).parallel(y).unroll(x,8);
        part.bound(x,0,WIDTH).bound(y,0,HEIGHT).parallel(y).unroll(x,8).loop_split();
        compare(prefix + "_b_p1_u8", norm, part, check, k);
    }  else if (id == "_p1_u8") {
        Func norm(prefix + "_p1_u8"), part(prefix + "_p1_u8_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.parallel(y).unroll(x,8);
        part.parallel(y).unroll(x,8).loop_split();
        compare(prefix + "_p1_u8", norm, part, check, k);
    }  else if (id == "_b_p4_u8") {
        Func norm(prefix + "_b_p4_u8"), part(prefix + "_b_p4_u8_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.bound(x,0,WIDTH).bound(y,0,HEIGHT).split(y,y,yi,4).parallel(y).unroll(x,8);
        part.bound(x,0,WIDTH).bound(y,0,HEIGHT).split(y,y,yi,4).parallel(y).unroll(x,8).loop_split();
        compare(prefix + "_b_p4_u8", norm, part, check, k);
    }  else if (id == "_p4_u8") {
        Func norm(prefix + "_p4_u8"), part(prefix + "_p4_u8_s");
        norm(x,y) = e;
        part(x,y) = e;
        norm.split(y,y,yi,4).parallel(y).unroll(x,8);
        part.split(y,y,yi,4).parallel(y).unroll(x,8).loop_split();
        compare(prefix + "_p4_u8", norm, part, check, k);
    }
}


# define CODEVIEW_1D 0
# define CODEVIEW_2D 0
# define SPEED_CLAMP 0
# define SPEED_CLAMP_2 1
# define SPEED_CLAMP_BDR 0
# define SPEED_MOD 0
# define SPEED_SELECT 0
# define SPEED_SELECT_CLAMP 0
# define SPEED_WRAP 0
# define SPEED_CONSTANT 0

std::string test_idlist_all[] = { "_", "_b", "_v8", /* "_v8_m", */ "_b_v8", "_b_v8_m", 
    "_p1", "_b_p1", "_p4", "_b_p4", "_p8", "_b_p8", "_p16", "_b_p16", 
    "_p1_u2", "_b_p1_u2", "_p1_u4", "_b_p1_u4", "_p1_u8", "_b_p1_u8", "_p4_u8", "_b_p4_u8", "" };
# define IDLIST test_idlist_all

std::string one_id[] = { "_v8", "" };
//# define IDLIST one_id    

std::string auto_idlist[] = { "_v8", "_b_v8", "_p1", "_p4", "_p1_v8", "" };

main (int argc, char **argv) {
    auto_test = argc < 2;
    
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
    
    std::string *idlist = IDLIST;
    if (auto_test) idlist = auto_idlist;
    
    Halide::global_options.loop_split = true;
    Halide::global_options.loop_split_all = false; // Never have loop split as the default
    std::cout << global_options;

    const int max_diag = 9;

    if (CODEVIEW_1D && ! auto_test) {
        const int the_diag = 2;
        // 1-D cases for code viewing and analysis.
        for (int k = the_diag; k <= the_diag; k++) {
            std::ostringstream ss;
            ss << "f1d_" << k;
            Expr e = input1d(clamp(x-k,0,WIDTH-1)) + input1d(clamp(x+k,0,WIDTH-1));
            Func s_v8(ss.str() + "_s_v8");
            s_v8(x) = e;
            s_v8.vectorize(x,8).loop_split();
            s_v8.compile_jit();
            Func b_s_v8(ss.str() + "_b_s_v8");
            b_s_v8(x) = e;
            b_s_v8.bound(x,0,WIDTH-1).vectorize(x,8).loop_split();
            //b_s_v8.compile_jit();
        }
    }
    
    if (CODEVIEW_2D && ! auto_test) {
        const int the_diag = 2;
        // 2-D cases for code viewing and analysis.
        for (int k = the_diag; k <= the_diag; k++) {
            std::ostringstream ss;
            ss << "f2d_" << k;
            Expr e = input(clamp(x-k,0,WIDTH-1),clamp(y-k,0,WIDTH-1)) + input(clamp(x+k,0,WIDTH-1),clamp(y+k,0,WIDTH-1));
            Func s_p4_u8(ss.str() + "_s_p4_u8");
            s_p4_u8(x,y) = e;
            s_p4_u8.split(y,y,yi,4).parallel(y).unroll(x,8).loop_split(x, true).loop_split(y,true);
            s_p4_u8.compile_jit();
        }
    }
    
    // The code generated in this loop consists of diagonal sum: in(x-k,y-k) + in(x+k,y+k)
    // with various forms of border handling.
    // SPEED_CLAMP: clamp() applied to index expressions
    // SPEED_CLAMP_2: clamp() applied to index expressions at two levels, with compute_root
    //    on the lower level so it tests the loop_split_all() schedule.
    // SPEED_CLAMP_BDR: clamp() applied to index expressions as inlined function (should always
    //    perform the same as SPEED_CLAMP)
    // SPEED_MOD: mod applied to index expressions as an inlined function.
    // SPEED_SELECT: Uses select() to return a constant out of bounds, and mod to restrict the index
    //    expression.
    // SPEED_SELECT_CLAMP: As SPEED_SELECT but restricts index expression using clamp()
    // SPEED_WRAP: Uses Border::wrap
    // SPEED_CONSTANT: Uses Border::constant
    for (int j = 0; idlist[j] != ""; j++) {
        // Note: Do not use k=1..8 because the border is only one unit wide (or one vector)
        // and when that happens the before and after loops degenerate to LetStmt when bound is applied.
        // When the before or after loop degenerates, it cannot be recognised as not being part of the outer main loop
        // so then loop splitting is detected as not being effective.
        // Start at k=9 so that the before and after loops do not degenerate.
        for (int k = 9; k <= max_diag; k++) {
            if (SPEED_CLAMP && ! auto_test) {
                std::ostringstream ss;
                ss << "clamp_" << k;
                Expr e = input(clamp(x-k,0,WIDTH-1),clamp(y-k,0,HEIGHT-1)) + input(clamp(x+k,0,WIDTH-1),clamp(y+k,0,HEIGHT-1));
                test(ss.str(),idlist[j],e,k,WIDTH-1-k,k,HEIGHT-1-k, check_clamp, k);
            }

            if (SPEED_CLAMP_2 || auto_test) {
                std::ostringstream ss;
                ss << "clamp2_" << k;
                Func f;
                f(x,y) = input(clamp(x-k,0,WIDTH-1),clamp(y-k,0,HEIGHT-1)) + input(clamp(x+k,0,WIDTH-1),clamp(y+k,0,HEIGHT-1));
                f.compute_root();
                Expr e = f(clamp(x-k,0,WIDTH-1),clamp(y-k,0,HEIGHT-1)) + f(clamp(x+k,0,WIDTH-1),clamp(y+k,0,HEIGHT-1));
                test(ss.str(),idlist[j],e,k,WIDTH-1-k,k,HEIGHT-1-k, check_clamp_2, k, 2);
            }

            if (SPEED_CLAMP_BDR && ! auto_test) {
                std::ostringstream ss;
                ss << "clamp_bdr_" << k;
                Func b;
                b(x,y) = input(clamp(x,0,WIDTH-1),clamp(y,0,WIDTH-1));
                Expr e = b(x-k,y-k) + b(x+k,y+k);
                test(ss.str(),idlist[j],e,k,WIDTH-1-k,k,HEIGHT-1-k, check_clamp, k);
            }

            if (SPEED_MOD || auto_test) {
                std::ostringstream ss;
                ss << "mod_" << k;
                Func b;
                b(x,y) = input(x%WIDTH,y%HEIGHT);
                Expr e = b(x-k,y-k) + b(x+k,y+k);
                test(ss.str(),idlist[j],e,k,WIDTH-1-k,k,HEIGHT-1-k, check_wrap, k);
            }

            if (SPEED_SELECT || auto_test) {
                std::ostringstream ss;
                ss << "smod_" << k;
                Func b;
                int c = 1;
                b(x,y) = select(x<0,c,select(x>WIDTH-1,c,
                         select(y<0,c,select(y>HEIGHT-1,c,input(x%WIDTH,y%HEIGHT)))));
                Expr e = b(x-k,y-k) + b(x+k,y+k);
                test(ss.str(),idlist[j],e,k,WIDTH-1-k,k,HEIGHT-1-k, check_const, k);
            }

            if (SPEED_SELECT_CLAMP && ! auto_test) {
                std::ostringstream ss;
                ss << "sclamp_" << k;
                Func b;
                int c = 1;
                b(x,y) = select(x<0,c,select(x>WIDTH-1,c,
                         select(y<0,c,select(y>HEIGHT-1,c,input(clamp(x,0,WIDTH-1),clamp(y,0,WIDTH-1))))));
                Expr e = b(x-k,y-k) + b(x+k,y+k);
                test(ss.str(),idlist[j],e,k,WIDTH-1-k,k,HEIGHT-1-k, check_const, k);
            }

            if (SPEED_WRAP || auto_test) {
                std::ostringstream ss;
                ss << "wrap_" << k;
                Func b = Border::wrap(input);
                Expr e = b(x-k,y-k) + b(x+k,y+k);
                test(ss.str(),idlist[j],e,k,WIDTH-1-k,k,HEIGHT-1-k, check_wrap, k);
            }
 
            if (SPEED_CONSTANT || auto_test) {
                std::ostringstream ss;
                ss << "const_" << k;
                Func b = Border::constant(1)(input);
                Expr e = b(x-k,y-k) + b(x+k,y+k);
                test(ss.str(),idlist[j],e,k,WIDTH-1-k,k,HEIGHT-1-k, check_const, k);
            }
        }
    }
    
    printf ("Slowest: %7.3f %s\n", slowest_time, slowest_id.c_str());
    printf ("Fastest: %7.3f %s\n", fastest_time, fastest_id.c_str());

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
