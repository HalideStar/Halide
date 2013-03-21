// Globals defined for border handling are defined within this module
#define BORDER_EXTERN 
#define BORDER_EXTERN_INIT(decl,init) decl = init
#define BORDER_EXTERN_CONSTRUCTOR(decl,args) decl args
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
namespace Internal {
template<>
EXPORT Internal::RefCount &ref_count<Border::BorderBase>(const Border::BorderBase *f) {return f->ref_count;}

template<>
EXPORT void destroy<Border::BorderBase>(const Border::BorderBase *f) {delete f;}
}

namespace Border {
// Core border function builder.
// Given a vector of border functions, apply then to the corresponding dimensions of f.
// Any missing dimensions are assumed to be Border::none as the handler.
static Func border_builder(vector<BorderFunc> borderfuncs, Func f) {
    Func g("border_" + f.name());
    Expr expr;
    
    assert((int) borderfuncs.size() <= f.dimensions() && "More border functions specified than dimensions of the function");
    while ((int) borderfuncs.size() < f.dimensions()) {
        borderfuncs.push_back(Border::none); // Pad missing border functions with Border::none
    }
    
    // Apply each border function to the corresponding dimension.
    // Build expression f(vector<Expr>) and build args list for g.
    vector<Var> g_args;
    vector<Expr> f_args;
    for (int i = 0; i < f.dimensions(); i++) {
        // Build the list of arguments for g.
        g_args.push_back(Var::gen(i));
        // Build the expression (..., BorderFunc.indexExpr(x, min, max), ...)
        f_args.push_back(borderfuncs[i].indexExpr(Var::gen(i), f.valid().min(i), f.valid().max(i)));
    }
    // Apply the subscripts to the function f.
    expr = f(f_args);
    // Insert value expressions for each of the border functions.
    for (int i = 0; i < f.dimensions(); i++) {
        expr = borderfuncs[i].valueExpr(expr, Var::gen(i), f.valid().min(i), f.valid().max(i));
    }
    Internal::log(4) << "Border handled expression: " << expr << "\n";
    g(g_args) = expr;
    return g;
}

// Build general border function from individual border functions.
BorderFunc border(BorderFunc h1) {
    return BorderFunc(new BorderGeneral(Internal::vec(h1)));
}

BorderFunc border(BorderFunc h1, BorderFunc h2) {
    return BorderFunc(new BorderGeneral(Internal::vec(h1, h2)));
}

BorderFunc border(BorderFunc h1, BorderFunc h2, BorderFunc h3) {
    return BorderFunc(new BorderGeneral(Internal::vec(h1, h2, h3)));
}

BorderFunc border(BorderFunc h1, BorderFunc h2, BorderFunc h3, BorderFunc h4) {
    return BorderFunc(new BorderGeneral(Internal::vec(h1, h2, h3, h4)));
}

// Apply border handler functions to corresponding dimensions.
Func BorderGeneral::operator()(Func f) {
    // Use the generic border function builder.
    return border_builder(borderfuncs, f);
}

// In the current BorderBase-derived class, build a BorderFunc that references
// a particular dimension.
BorderFunc BorderBase::dim(int d) { 
    if (d > 0) {
        return BorderFunc(new BorderIndex(BorderFunc(this), d)); 
    }
    else {
        return this; 
    }
}

Func BorderBase::operator()(Func in) {
    std::vector<BorderFunc> args;
    for (int i = 0; i < in.dimensions(); i++) {
        // Get the border function for the particular dimension.
        args.push_back(this->dim(i));
    }
    // Use the generic border function builder.
    return border_builder(args, in);
}

BorderFunc constant(Expr k) { 
    return BorderFunc(new BorderConstant(k)); 
}

BorderFunc tile(Expr t1) { 
    return BorderFunc(new BorderTile(vec(t1))); 
}

BorderFunc tile(Expr t1, Expr t2) { 
    return BorderFunc(new BorderTile(vec(t1, t2))); 
}

BorderFunc tile(Expr t1, Expr t2, Expr t3) { 
    return BorderFunc(new BorderTile(vec(t1, t2, t3))); 
}

BorderFunc tile(Expr t1, Expr t2, Expr t3, Expr t4) { 
    return BorderFunc(new BorderTile(vec(t1, t2, t3, t4))); 
}

BorderFunc BorderFunc::dim(int d) {
    return BorderFunc(new BorderIndex(*this, d)); 
}

// End namespace Border
}

namespace Internal {

// Some simple border function tests.
void border_test() {
    Var x("x");
    Func init("init"), f("f");

    init(x) = x;
    init.set_valid() = Domain("x", false, 3, 5);
    
    // Test application of replicate border handling
    f(x) = Border::replicate(init)(x);
    Image<int> out = f.realize(10);
    
    assert(equal(f.valid().min(0), 3) && equal(f.valid().max(0), 5) && "Border function test failed: replication domain");
    assert(equal(f.valid().exact(0), const_true()) && "Border function test failed: inexact result");
    
    for (int i = 0; i < 10; i++) {
        if (out(i) != ((i < 3) ? 3 : (i > 5 ? 5 : i))) {
            assert(0 && "Border function test failed: replication values");
        }
    }
    
    std::cout << "Border function tests passed\n";
}


}
}
