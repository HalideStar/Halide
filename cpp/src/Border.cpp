// Globals defined for border handling are defined within this module
// BORDER_EXTERN_CONSTRUCTOR: Declare a globally visible object with arguments to its constructor
#define BORDER_EXTERN_CONSTRUCTOR(decl,args) decl args
#include "Border.h"

#include "IR.h"
#include "IREquality.h"
#include "IRPrinter.h"
#include "Func.h"
#include "Log.h"
#include "Util.h"
#include "Simplify.h"

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
        // In case the min or max are undefined, set them to the extremes of the type of Var.
        // This means that the relevant bound is not enforced.
        Expr min = f.valid().min(i);
        Expr max = f.valid().max(i);
        Expr v = Var::gen(i);
        if (! min.defined()) { min = v.type().min(); }
        if (! max.defined()) { max = v.type().max(); }
        f_args.push_back(borderfuncs[i].indexExpr(v, min, max));
    }
    // Apply the subscripts to the function f.
    expr = f(f_args);
    // Insert value expressions for each of the border functions.
    for (int i = 0; i < f.dimensions(); i++) {
        // As above, undefined bounds are mapped to the extremes of a 32-bit integer.
        Expr min = f.valid().min(i);
        Expr max = f.valid().max(i);
        Expr v = Var::gen(i);
        if (! min.defined()) { min = v.type().min(); }
        if (! max.defined()) { max = v.type().max(); }
        expr = borderfuncs[i].valueExpr(expr, v, min, max);
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

Expr BorderFunc::indexExpr(Expr expr, Expr min, Expr max) { 
    assert(contents.ptr && "Undefined border function"); 
    return contents.ptr->indexExpr(0, expr, min, max); 
}
Expr BorderFunc::valueExpr(Expr value, Expr expr, Expr min, Expr max) { 
    assert(contents.ptr && "Undefined border function"); 
    return contents.ptr->valueExpr(0, value, expr, min, max); 
}

Expr BorderFunc::indexExpr(int dim, Expr expr, Expr min, Expr max) { 
    assert(contents.ptr && "Undefined border function"); 
    return contents.ptr->indexExpr(dim, expr, min, max); 
}
Expr BorderFunc::valueExpr(int dim, Expr value, Expr expr, Expr min, Expr max) { 
    assert(contents.ptr && "Undefined border function"); 
    return contents.ptr->valueExpr(dim, value, expr, min, max); 
}

BorderFunc BorderFunc::dim(int d) {
    return BorderFunc(new BorderIndex(*this, d)); 
}

Func BorderFunc::operator()(Func in) { 
    assert(contents.ptr != NULL && "BorderFunc has NULL contents");
    return contents.ptr->operator()(in); 
}

Expr BorderIndex::indexExpr(int _dim, Expr expr, Expr min, Expr max) { 
    return base.indexExpr(_dim + dim, expr, min, max); 
}

Expr BorderIndex::valueExpr(int _dim, Expr value, Expr expr, Expr min, Expr max) { 
    return base.valueExpr(_dim + dim, value, expr, min, max); 
}

Expr BorderValueBase::indexExpr(int dim, Expr expr, Expr xmin, Expr xmax) { 
    return clamp(expr, xmin, xmax); 
}

Expr BorderConstant::valueExpr(int dim, Expr value, Expr expr, Expr min, Expr max) {
    assert(constant.defined() && "Border::constant requires constant value to be specified"); 
    assert(expr.defined() && "Border::constant - undefined index expression");
    assert(value.defined() && "Border::constant - undefined value expression");
    value = select(expr > max, cast(value.type(), constant), value);
    value = select(expr < min, cast(value.type(), constant), value);
    return value; 
}

Expr BorderTile::indexExpr(int dim, Expr expr, Expr min, Expr max) {
    assert(tile.size() > 0 && "BorderTile requires at least one tile dimension");
    dim = dim % ((int) tile.size());
    return new Internal::Clamp(Internal::Clamp::Tile, expr, min, max, tile[dim]); 
}

    // End namespace Border
}

namespace Internal {

namespace {
bool check(Func f, Expr e, Expr s) {
    if (! equal(f.value(), e)) {
        std::cout << "Border handler construction failed\n";
        std::cout << "Expected: " << e << "\n";
        std::cout << "Actual:   " << f.value() << "\n";
        return false;
    }
    // Test whether the simplifiers does what we need it to do.
    // It gets applied during compilation, so this is only an
    // approximate test and does not guarantee correct compilation.
    Expr fs = simplify(f.value());
    if (! equal(fs, s)) {
        std::cout << "Border handler simplification failed\n";
        std::cout << "Expected: " << s << "\n";
        std::cout << "Actual:   " << fs << "\n";
        return false;
    }
    return true;
}
}

// Some simple border function tests.
void border_test() {
    bool success = true;
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
    
    // Test what happens with undefined domain limit.
    Func in2("in2"), f2("f2");
    in2(x) = x;
    in2.set_valid() = Domain("x", false, Expr(), 15);
    
    success &= check(Border::replicate(in2), 
                     in2(max(min(Var::gen(0), 15), Int(32).min())), 
                     in2(min(Var::gen(0), 15)));
    success &= check(Border::wrap(in2), 
                     in2(new Clamp(Clamp::Wrap, Var::gen(0), Int(32).min(), 15)), 
                     in2(new Clamp(Clamp::Wrap, Var::gen(0), Int(32).min(), 15)));
    success &= check(Border::constant(3)(in2), 
                     select(Var::gen(0) < Int(32).min(), 3, select(Var::gen(0) > 15,
                     3, in2(max(min(Var::gen(0), 15), Int(32).min())))), 
                     select(15 < Var::gen(0), 3, in2(min(Var::gen(0), 15))));
    
    assert (success && "Border function tests failed\n");
    
    std::cout << "Border function tests passed\n";
}


}
}
