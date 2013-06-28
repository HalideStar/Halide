#include <stdio.h>
#include <Halide.h>

using namespace Halide;


#define MAXX 14
#define MAXY 15
#define LOX 4
#define HIX 9
#define LOY 4
#define HIY 10
#define MUL 100

int expect_raw(int x, int y) { return x + y * MUL; }

int do_replicate(int x, int lox, int hix) { return (x < lox ? lox : (x > hix ? hix: x)); }
int expect_replicate(int x, int y) { return do_replicate(x,LOX,HIX) + do_replicate(y,LOY,HIY)*MUL; }
int expect_kernel_replicate(int x, int y) { 
    return (do_replicate(x-1,LOX,HIX) + do_replicate(y,LOY,HIY)*MUL +
            do_replicate(x+1,LOX,HIX) + do_replicate(y,LOY,HIY)*MUL) / 2; }

int do_wrap(int x, int lox, int hix) { 
    // Reference implementation by repeated wrap
    // This ensures that we do not run into problems with implementation of modulus etc.
    // Since it is only used for testing, we can afford the execution time.
    while (x < lox) x += hix - lox + 1;
    while (x > hix) x -= hix - lox + 1;
    return x;
}
int expect_wrap(int x, int y) { return do_wrap(x,LOX,HIX) + do_wrap(y,LOY,HIY)*MUL; }

int do_reflect(int x, int lox, int hix) {
    // Reference implementation by repeated reflection
    while (x < lox || x > hix) {
        if (x < lox) x = (lox - x - 1 + lox);
        if (x > hix) x = (hix - x + 1 + hix);
    }
    return x;
}
int expect_reflect(int x, int y) { return do_reflect(x,LOX,HIX) + do_reflect(y,LOY,HIY)*MUL; }

int do_reflect101(int x, int lox, int hix) {
    // Reference implementation by repeated reflection
    while (x < lox || x > hix) {
        if (x < lox) x = (lox - x + lox);
        if (x > hix) x = (hix - x + hix);
    }
    return x;
}
int expect_reflect101(int x, int y) { return do_reflect101(x,LOX,HIX) + do_reflect101(y,LOY,HIY)*MUL; }

int expect_wrap_reflect(int x, int y) { return do_wrap(x,LOX,HIX) + do_reflect(y,LOY,HIY)*MUL; }

int expect_constant0(int x, int y) { return (x < LOX || x > HIX || y < LOY || y > HIY) ? 0 : x + y * MUL; }

int do_tile(int x, int lox, int hix, int tile) {
    while (x < lox) x += tile;
    while (x > hix) x -= tile;
    return x;
}
int expect_tile23(int x, int y) { return do_tile(x,LOX,HIX,2) + do_tile(y,LOY,HIY,3)*MUL; }

static bool check_domain (std::string name, std::string dname, Domain d, Domain expect) {
    bool success = true;
    if (d.intervals.size() != expect.intervals.size()) {
        success = false;
        std::cout << name <<": " << dname << ": Domain sizes differ\n";
    }
    for (size_t i = 0; i < d.intervals.size() && i < expect.intervals.size(); i++) {
        if (! equal(d.intervals[i].interval.min, expect.intervals[i].interval.min)) {
            std::cout << name << ": " << dname << "[" << i << "]: Expected imin: " << expect.intervals[i].interval.min << "  Got: " << d.intervals[i].interval.min << "\n";
            success = false;
        }
        if (! equal(d.intervals[i].interval.max, expect.intervals[i].interval.max)) {
            std::cout << name << ": " << dname << "[" << i << "]: Expected imax: " << expect.intervals[i].interval.max << "  Got: " << d.intervals[i].interval.max << "\n";
            success = false;
        }
        if (d.intervals[i].exact != expect.intervals[i].exact) {
            std::cout << name << ": " << dname << "[" << i << "]: Expected exact: " << expect.intervals[i].exact << "  Got: " << d.intervals[i].exact << "\n";
            success = false;
        }
    }
    return success;
}

static void check(std::string name, Expr expr, int (*expected)(int, int), Domain *valid = 0, Domain *computable = 0) {
    Internal::log(1,"DOMINF") << "\nChecking " << name << "\n";
    bool success = true;
    Func c("check");
    c = expr;
    if (expected != 0) {
        Image<int> out = c.realize(MAXX, MAXY);
        for (int i = 0; i < MAXX; i++) {
            for (int j = 0; j < MAXY; j++) {
                int expect = (*expected)(i,j);
                if (out(i,j) != expect) {
                    std::cout << "Incorrect " << name << " (" << i << "," << j <<")  Expected: " << expect << "   Got: " << out(i,j) << "\n";
                    assert(0 && "Border handling test failed");
                }
            }
        }
    }
    
    if (valid != 0) {
        success &= check_domain (name, "Valid", c.valid(), *valid);
    }
    if (computable != 0) {
        success &= check_domain (name, "Computable", c.computable(), *computable);
    }
    
    assert(success && "Border handling test failed");
}

static void check(std::string name, Expr expr, int (*expected)(int, int), Domain valid, Domain computable) {
    check (name, expr, expected, &valid, &computable);
}

void border_test() {
    Var x("x"), y("y");
    Func init("init");

    // Define a function to get some data for testing.
    init(x,y) = x + y * MUL;
    
    // Manually set all the domains of init.
    Domain initd(LOX, HIX, LOY, HIY);
    init.set_valid() = initd;
    init.set_computable() = initd;
    
    // First suite of tests: Check the implementation of the different border functions
    // when they are invoked using Border::border.
    check("raw", init, expect_raw);
    check("Border::replicate", Border::border(Border::replicate, Border::replicate)(init), expect_replicate);
    check("Border::wrap", Border::border(Border::wrap, Border::wrap)(init), expect_wrap);
    check("Border::reflect", Border::border(Border::reflect, Border::reflect)(init), expect_reflect);
    check("Border::reflect101", Border::border(Border::reflect101, Border::reflect101)(init), expect_reflect101);
    check("Border::constant(0)", Border::border(Border::constant(0), Border::constant(0))(init), expect_constant0);
    check("Border::tile(2),Border::tile(3)", Border::border(Border::tile(2), Border::tile(3))(init), expect_tile23); 
    // Check the BorderIndex class
    check("Border::tile(2,3)", Border::border(Border::tile(2,3).dim(0), Border::tile(2,3).dim(1))(init), expect_tile23); 
    
    // Check mixed border functions
    check("Border::wrap,Border::reflect", Border::border(Border::wrap, Border::reflect)(init), expect_wrap_reflect);
    
    // Check direct application of border functions
    check("Border::replicate()", Border::replicate(init), expect_replicate);
    check("Border::wrap()", Border::wrap(init), expect_wrap);
    check("Border::reflect()", Border::reflect(init), expect_reflect);
    check("Border::reflect101()", Border::reflect101(init), expect_reflect101);
    check("Border::constant(0)()", Border::constant(0)(init), expect_constant0);
    check("Border::tile(2,3)()", Border::tile(2,3)(init), expect_tile23); 
    
    // Test with a realized image as the input.
    Func input("input");;
    input(x,y) = (x + LOX) + (y + LOY) * MUL;
    Image<int> in = input.realize(6, 7);
    
    // Now, build a function that accesses outside the bounds of in.
    Func g("g");
    g(x,y) = in(x-LOX,y-LOY); // This is a shift: in is valid on 0,5 0,6  g is valid on 4,9  4,10
    // Test the border handlers and check the computed domains.
    check("Border::replicate()", Border::replicate(g), expect_replicate, 
        Domain(LOX, HIX, LOY, HIY), 
        Domain(Expr(), Expr(), Expr(), Expr()));
    check("Border::wrap()", Border::wrap(g), expect_wrap, 
        Domain(LOX, HIX, LOY, HIY), 
        Domain(Expr(), Expr(), Expr(), Expr()));
    check("Border::reflect()", Border::reflect(g), expect_reflect, 
        Domain(LOX, HIX, LOY, HIY), 
        Domain(Expr(), Expr(), Expr(), Expr()));
    check("Border::reflect101()", Border::reflect101(g), expect_reflect101, 
        Domain(LOX, HIX, LOY, HIY), 
        Domain(Expr(), Expr(), Expr(), Expr()));
    check("Border::constant(0)()", Border::constant(0)(g), expect_constant0, 
        Domain(LOX, HIX, LOY, HIY), 
        Domain(Expr(), Expr(), Expr(), Expr()));
    check("Border::tile(2,3)()", Border::tile(2,3)(g), expect_tile23, 
        Domain(LOX, HIX, LOY, HIY), 
        Domain(Expr(), Expr(), Expr(), Expr())); 
        
    // Test kernel notation
    
    // Build a border function applied to g.
    Func border("border");
    border = Border::replicate(g);
    
    // Kernel function computed from function with borders.
    check("kernel", (border[-1][0] + border[1][0]) / 2, expect_kernel_replicate,
        Domain(LOX, HIX, LOY, HIY),
        Domain(Expr(), Expr(), Expr(), Expr()));
        
    // Build a kernel function where there is no border handling, so valid domain is restricted by
    // the computable domain.
    check("kernel2", (g[-1][0] + g[1][0]) / 2, 0,
        Domain(LOX+1, HIX-1, LOY, HIY),
        Domain(LOX+1, HIX-1, LOY, HIY));
    
    // An example using kernel mode computation with border handling then border none on top of
    // that.  The second layer restricts the computable domain and hence the valid domain.
    Func kernel("kernel"), none("none");
    kernel = (border[-1][-1] + border[1][1]) / 2;
    none = Border::none(kernel);
    check("kernel+none", (none[-2][-1] + none[1][1]) / 2, 0,
        Domain(LOX+2, HIX-1, LOY+1, HIY-1),
        Domain(LOX+2, HIX-1, LOY+1, HIY-1));
}


int main(int argc, char **argv) {
    border_test();
    
    printf("Success!\n");
    return 0;
}
