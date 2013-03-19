// Globals defined for border handling are defined within this module
#define BORDER_EXTERN 
#define BORDER_EXTERN_INIT(decl,init) decl = init
#include "Border.h"

#include "IR.h"
#include "IREquality.h"
#include "IRPrinter.h"
#include "Func.h"
#include "Log.h"
#include "Util.h"

#include <vector>
#include <string>

using std::vector;
using std::string;

namespace Halide {
namespace Border {
// Core border handler implementation.
// Given a vector of border handlers, apply then to the corresponding dimensions of f.
// Any missing dimensions are assumed to be Border::none as the handler.
Func border(vector<BorderHandler> handlers, Func f) {
    Func g("border_" + f.name());
    Expr expr;
    
    assert((int) handlers.size() <= f.dimensions() && "More border handlers specified than dimensions of the function");
    while ((int) handlers.size() < f.dimensions())
        handlers.push_back(Border::none); // Pad missing border handlers with Border::none
    
    // Apply each border handler to the corresponding dimension.
    // Build expression f(vector<Expr>) and build args list for g.
    vector<Var> g_args;
    vector<Expr> f_args;
    for (int i = 0; i < f.dimensions(); i++) {
        // Build the list of arguments for g.
        g_args.push_back(Var::gen(i));
        // Build the expression (..., BorderHandler.indexExpr(x, min, max), ...)
        f_args.push_back(handlers[i].indexExpr(Var::gen(i), f.valid().min(i), f.valid().max(i)));
    }
    // Apply the subscripts to the function f.
    expr = f(f_args);
    // Insert value expressions for each of the border handlers.
    for (int i = 0; i < f.dimensions(); i++) {
        expr = handlers[i].valueExpr(expr, Var::gen(i), f.valid().min(i), f.valid().max(i));
    }
    Internal::log(4) << "Border handled expression: " << expr << "\n";
    g(g_args) = expr;
    return g;
}

Func border(BorderHandler h1, Func f) {
    return border(Internal::vec(h1), f);
}

Func border(BorderHandler h1, BorderHandler h2, Func f) {
    return border(Internal::vec(h1, h2), f);
}

Func border(BorderHandler h1, BorderHandler h2, BorderHandler h3, Func f) {
    return border(Internal::vec(h1, h2, h3), f);
}

Func border(BorderHandler h1, BorderHandler h2, BorderHandler h3, BorderHandler h4, Func f) {
    return border(Internal::vec(h1, h2, h3, h4), f);
}


BorderBase *BorderBase::dim(int d) { 
    if (d > 0) {
        return new BorderIndex(this, d); 
    }
    else {
        return this; 
    }
}

Func BorderBase::operator()(Func in) {
    std::vector<BorderHandler> args;
    for (int i = 0; i < in.dimensions(); i++) {
        // Get the border handler for the particular dimension.
        args.push_back(this->dim(i));
    }
    // Use the generic border handler function.
    return border(args, in);
}

}

namespace Internal {

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
        if (! equal(d.intervals[i].imin, expect.intervals[i].imin)) {
            std::cout << name << ": " << dname << "[" << i << "]: Expected imin: " << expect.intervals[i].imin << "  Got: " << d.intervals[i].imin << "\n";
            success = false;
        }
        if (! equal(d.intervals[i].imax, expect.intervals[i].imax)) {
            std::cout << name << ": " << dname << "[" << i << "]: Expected imax: " << expect.intervals[i].imax << "  Got: " << d.intervals[i].imax << "\n";
            success = false;
        }
        if (! equal(d.intervals[i].poison, expect.intervals[i].poison)) {
            std::cout << name << ": " << dname << "[" << i << "]: Expected poison: " << expect.intervals[i].poison << "  Got: " << d.intervals[i].poison << "\n";
            success = false;
        }
    }
    return success;
}

static void check(std::string name, Expr expr, int (*expected)(int, int), Domain *valid = 0, Domain *computable = 0) {
    log(1) << "Checking " << name << "\n";
    bool success = true;
    Func c("check");
    c = expr;
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

    init(x,y) = x + y * MUL;
    // Manually set the valid and computable domains of init.
    Domain initd("x", false, LOX, HIX, "y", false, LOY, HIY);
    init.valid() = initd;
    init.computable() = initd;
    
    check("raw", init, expect_raw);
    check("Border::replicate", border(Border::replicate, Border::replicate, init), expect_replicate);
    check("Border::wrap", border(Border::wrap, Border::wrap, init), expect_wrap);
    check("Border::reflect", border(Border::reflect, Border::reflect, init), expect_reflect);
    check("Border::reflect101", border(Border::reflect101, Border::reflect101, init), expect_reflect101);
    check("Border::constant(0)", border(Border::constant(0), Border::constant(0), init), expect_constant0);
    check("Border::tile(2),Border::tile(3)", border(Border::tile(2), Border::tile(3), init), expect_tile23); 
    check("Border::tile(2,3)", border(Border::tile(2,3).dim(0), Border::tile(2,3).dim(1), init), expect_tile23); 
    
    check("Border::wrap,Border::reflect", border(Border::wrap, Border::reflect, init), expect_wrap_reflect);
    
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
    
    // Now, build a function that accesses out of the bounds of in.
    Func g("g");
    g(x,y) = in(x-LOX,y-LOY); // in is valid on 0,5 0,6  g is valid on 4,9  4,10
    check("Border::replicate()", Border::replicate(g), expect_replicate);
    check("Border::wrap()", Border::wrap(g), expect_wrap, 
        Domain("x", false, LOX, HIX, "y", false, LOY, HIY), 
        Domain("x", false, Expr(), Expr(), "y", false, Expr(), Expr()));
    check("Border::reflect()", Border::reflect(g), expect_reflect, 
        Domain("x", false, LOX, HIX, "y", false, LOY, HIY), 
        Domain("x", false, Expr(), Expr(), "y", false, Expr(), Expr()));
    check("Border::reflect101()", Border::reflect101(g), expect_reflect101, 
        Domain("x", false, LOX, HIX, "y", false, LOY, HIY), 
        Domain("x", false, Expr(), Expr(), "y", false, Expr(), Expr()));
    check("Border::constant(0)()", Border::constant(0)(g), expect_constant0, 
        Domain("x", false, LOX, HIX, "y", false, LOY, HIY), 
        Domain("x", false, Expr(), Expr(), "y", false, Expr(), Expr()));
    check("Border::tile(2,3)()", Border::tile(2,3)(g), expect_tile23, 
        Domain("x", false, LOX, HIX, "y", false, LOY, HIY), 
        Domain("x", false, Expr(), Expr(), "y", false, Expr(), Expr())); 

    std::cout << "Border handling tests passed\n";
}

}
}
