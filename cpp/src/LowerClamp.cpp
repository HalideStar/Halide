#include "IR.h"
#include "IREquality.h"
#include "IROperator.h"
#include "IRMutator.h"
#include "IRPrinter.h"
#include "Log.h"
#include "Simplify.h"
#include "Var.h"
#include "Image.h"
#include "Func.h"
#include <iostream>
#include <set>
#include <sstream>

using std::string;
using std::vector;

namespace Halide {
namespace Internal {

// Lower clamp nodes into their implementations
// First cut: just a simple implementation
class LowerClamp : public IRMutator {

    using IRMutator::visit;

    void visit(const Clamp *op) {
        Expr op_a = mutate(op->a);
        Expr op_min = mutate(op->min);
        Expr op_max = mutate(op->max);
        Expr op_p1 = mutate(op->p1);
        Expr r, e;
        Expr adjust = op->type.is_float() ? Expr(0) : Expr(1);
        switch (op->clamptype) {
            case Clamp::None:
                expr = op_a;
                break;
            case Clamp::Replicate:
                expr = new Max(new Min(op_a, op_max), op_min);
                break;
            case Clamp::Wrap:
                // Integers wrap by modulus max-min+1.  Floats wrap by fmod max-min
                expr = (op_a - op_min) % (op_max - op_min + adjust) + op_min;
                break;
            case Clamp::Reflect:
                // Reflect.  
                // First Subtract op_min.
                // r = op_max - op_min + 1 (if float, ignore the +1)
                // e = Wrap into the range (0,2*r)
                // If greater than or equal to r, reflect. 
                //    For integers:
                //      r reflects to r-1.  2 * r - 1 reflects to 0.
                //      This is 2*r - 1 - e.
                //    For floats
                //      r reflects to r.  r * 2 reflects to 0.
                //      This is 2*r - e
                // Finally, add op_min back on.
                r = op_max - op_min + adjust;
                e = (op_a - op_min) % (2 * r);
# if LOWER_CLAMP_LATE
                // The following definition causes Halide to think there is an out of bounds access
                // because the interval analysis does not use the boolean to restrict the range of the 
                // true and false expressions (which is quite difficult to do).
                e = select(e < r, e, r * 2 - adjust - e);
#else
                // The following alternative implementation has more mod operations, but should pass the
                // analysis OK.
                e = select(e < r, (op_a - op_min) % r, r - adjust - (op_a - op_min) % r);
#endif
                expr = e + op_min;
                break;
            case Clamp::Reflect101:
                // Reflect101.  For float, this is the same as Reflect
                if (op->type.is_float()) {
                    expr = mutate(new Clamp(Clamp::Reflect, op_a, op_min, op_max));
                } else {
                    // First Subtract op_min.
                    // r = op_max - op_min
                    // e = Wrap into the range (0,2*r)
                    // If greater than r, reflect. 
                    //      r+1 reflects to r-1.  2 * r - 1 reflects to 1.
                    //      This is 2*r - e.
                    // Finally, add op_min back on.
                    r = op_max - op_min;
                    e = (op_a - op_min) % (2 * r);
#if LOWER_CLAMP_LATE
                    e = select(e <= r, e, r * 2 - e);
#else
                    // Early lowering. Require both the true and false expressions to
                    // be on the range (0,r) inclusive.
                    // If interval inference gets clever, it should see that
                    // e or (r*2-e) as appropriate is already on the range (0,r) inclusive
                    // and delete the mod (r+adjust)
                    e = select(e <= r, e % (r+adjust), (r * 2 - e) % (r+adjust));
#endif
                    expr = e + op_min;
                }
                break;
            case Clamp::Tile:
                // Integers tile by modulus p1.  Floats tile by fmod p1
#if LOWER_CLAMP_LATE
                expr = select(op_a < op_min, (op_a - op_min) % op_p1 + op_min, 
                       select(op_a > op_max, (op_a - op_max - adjust) % op_p1 + op_max + adjust - op_p1, op_a));
#else
                expr = select(op_a < op_min, (op_a - op_min) % op_p1 + op_min, 
                       select(op_a > op_max, (op_a - op_max - adjust) % op_p1 + op_max + adjust - op_p1, (op_a - op_min) % (op_max - op_min + adjust) + op_min));
#endif
                break;
            default:
                assert(0 && "Unknown clamp type in Clamp node");
                expr = op; // Failure condition; if assert is suppressed, retains original code
                break;
        }
    }
    
public:
    LowerClamp() {}
};

Expr lower_clamp(Expr e) {
    LowerClamp lower;
    return lower.mutate(e);
}
Stmt lower_clamp(Stmt s) {
    LowerClamp lower;
    return lower.mutate(s);
}

static void check(Expr e, Expr result) {
    Expr r = simplify(lower_clamp(e));
    Expr res = simplify(result);
    if (! equal(r, res)) {
        std::cout << "Clamp lowering failed\n";
        std::cout << "Expression: " << e << '\n';
        std::cout << "Expected:   " << result << '\n';
        std::cout << "Actual:     " << r << '\n';
    }
}

void lower_clamp_test() {

    Func reflect("reflect"), reflect101("reflect101"), tile("tile");
    Var x("x");
    Image<int> out;
    bool success = true;
    
    check(clamp(x,30,50), max(min(x,50),30));
    check( new Clamp(Clamp::Wrap, x, 30,50), (x - 30) % (21) + 30);
    
    // Test reflection by execution
    reflect(x) = lower_clamp(new Clamp(Clamp::Reflect, x, 30,50));
    out = reflect.realize(100);
    for (int i = 0; i < 100; i++) {
        // Do reflection the slow way.
        int v;
        v = i;
        while (v < 30 || v > 50) {
            if (v < 30) v = 29 - v + 30; // v=29 reflects to 30.
            if (v > 50) v = 51 - v + 50; // v=51 reflects to 50
        }
        if (out(i) != v) {
            std::cout << "Clamp lowering test failed for Reflect\n";
            std::cout << "x: " << i << "  reflect(x): " << out(i) << "  expected: " << v << '\n';
            success = false;
        }
    }

    // Test reflection by execution
    reflect101(x) = lower_clamp(new Clamp(Clamp::Reflect101, x, 30,50));
    out = reflect101.realize(100);
    for (int i = 0; i < 100; i++) {
        // Do reflection the slow way.
        int v;
        v = i;
        while (v < 30 || v > 50) {
            if (v < 30) v = 29 - v + 31; // v=29 reflects to 31.
            if (v > 50) v = 51 - v + 49; // v=51 reflects to 49
        }
        if (out(i) != v) {
            std::cout << "Clamp lowering test failed for Reflect101\n";
            std::cout << "x: " << i << "  reflect101(x): " << out(i) << "  expected: " << v << '\n';
            success = false;
        }
    }
    
    // Test tile by execution
    tile(x) = lower_clamp(new Clamp(Clamp::Tile, x, 30,50,3));
    out = tile.realize(100);
    for (int i = 0; i < 100; i++) {
        // Do tiling the slow way.
        int v;
        v = i;
        while (v < 30) v += 3;
        while (v > 50) v -= 3;
        if (out(i) != v) {
            std::cout << "Clamp lowering test failed for Tile\n";
            std::cout << "x: " << i << "  tile(x): " << out(i) << "  expected: " << v << '\n';
            success = false;
        }
    }
    
    if (! success)
        assert(0 && "Clamp lowering test failed");

    std::cout << "Clamp lowering test passed" << std::endl;
}

}
}
