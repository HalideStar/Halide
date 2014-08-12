#include "BoundsSimplify.h"
#include "BoundsAnalysis.h"
#include "IRCacheMutator.h"
#include "IR.h"
#include "IROperator.h"
#include "IREquality.h"
#include "IRPrinter.h"
#include "InfInterval.h"
#include "Util.h"
#include "Var.h"
#include "Log.h"
#include <iostream>
#include <vector>

using std::vector;

// Simplify using bounds analysis provided by IntervalAnal.
// Note: If there are variables remaining in the intervals, then
// they cannot be further resolved.  Therefore, proved() is appropriate.

namespace Halide { 
namespace Internal {

class BoundsSimplify : public IRCacheMutator {
    typedef IRCacheMutator Super;
    
    BoundsAnalysis bounds;
    
public:
    using Super::visit;

    void visit(const Mod *op) {
        InfInterval bounds_a = bounds.bounds(op->a);
        InfInterval bounds_b = bounds.bounds(op->b);
        if (proved(bounds_b.min > bounds_a.max) && proved(bounds_a.min >= 0)) {
            // The expression a is always in the bounds of the (positive) modulus b.
            expr = mutate(op->a);
        } else if (proved(bounds_b.max < bounds_a.min) && proved(bounds_a.max <= 0)) {
            // The expression a is always in the bounds of the negative modulus b.
            expr = mutate(op->b);
        } else {
            Super::visit(op);
        }
    }

	//LH
    void visit(const Clamp *op) {
        InfInterval bounds_a = bounds.bounds(op->a);
        InfInterval bounds_min = bounds.bounds(op->min);
        InfInterval bounds_max = bounds.bounds(op->max);
        if (op->clamptype == Clamp::None || (proved(bounds_min.max <= bounds_a.min) && 
            proved(bounds_max.min >= bounds_a.max))) {
            // The expression is always in bounds, so the clamp is not required at all.
            expr = mutate(op->a);
        } else {
            Super::visit(op);
        }
    }

    void visit(const Min *op) {
        //log(0) << "Min " << op->a << ", " << op->b << "\n";
        InfInterval bounds_a = bounds.bounds(op->a);
        InfInterval bounds_b = bounds.bounds(op->b);
        //log(0) << "    interval a " << bounds_a << "  b " << bounds_b << "\n";
        //log(0) << "    current context " << current_context() << "\n";
        if (proved(bounds_a.max <= bounds_b.min)) {
            expr = mutate(op->a);
        } else if (proved(bounds_b.max <= bounds_a.min)) {
            expr = mutate(op->b);
        } else {
            Super::visit(op);
        }
    }

    void visit(const Max *op) {
        //log(0) << "Max " << op->a << ", " << op->b << "\n";
        InfInterval bounds_a = bounds.bounds(op->a);
        InfInterval bounds_b = bounds.bounds(op->b);
        if (proved(bounds_a.min >= bounds_b.max)) {
            expr = mutate(op->a);
        } else if (proved(bounds_b.min >= bounds_a.max)) {
            expr = mutate(op->b);
        } else {
            Super::visit(op);
        }
    }

    void visit(const Select *op) {
        InfInterval bounds_cond = bounds.bounds(op->condition);
        if (is_one(bounds_cond.min)) {
            // Provably always true condition.
            expr = mutate(op->true_value);
        } else if (is_zero(bounds_cond.max)) {
            expr = mutate(op->false_value);
        } else {
            Super::visit(op);
        }
    }
          
# if 0
    void visit(const AssertStmt *op) {
        // Complex bounds can arise in assertions. Skip them.
        stmt = op;
        return;
    }
# endif

# if 0
    // For loop visit only to focus debugging.
    void visit(const For *op) {
        int old_debug_level = log::debug_level;
        
        // If it is the main loop, keep debug level, otherwise kill debug.
        if (op->partition.status != PartitionInfo::Main && 
            op->partition.status != PartitionInfo::Ordinary) {
            log::debug_level = -1;
            if (log::debug_level != old_debug_level) 
                log(-1) << "Debug level " << old_debug_level << " -> " << log::debug_level << "\n";
        }
        Super::visit(op);
        
        if (log::debug_level != old_debug_level) 
            log(-1) << "Debug level " << log::debug_level << " -> " << old_debug_level << "\n";
        log::debug_level = old_debug_level;
    }
# endif
};


Stmt bounds_simplify(Stmt s) {
    BoundsSimplify b;
    Stmt r = b.mutate(s); // Perform the mutation.
    return r;
}

Expr bounds_simplify(Expr e) {
    BoundsSimplify b;
    return b.mutate(e); // Perform the mutation.
}


namespace{
void check(Stmt a, Stmt b, int lo = 0, int hi = 10) {
    // Use loop to set intervals on the variables.
    Stmt fora = For::make("x", Expr(lo), Expr(hi - lo + 1), For::Serial, a);
    //Stmt forb = For::make("x", Expr(lo), Expr(hi - lo + 1), For::Serial, b);
    // Simplify a.
    Stmt simpler = bounds_simplify(fora);
    const For *result = simpler.as<For>();
    if (!equal(result->body, b)) {
        std::cout << std::endl << "Simplify bounds failure: " << std::endl;
        std::cout << "Input: " << a << std::endl;
		std::cout << "  where x is in the interval [" << lo << ", " << hi << "]" << std::endl;
        std::cout << "Output: " << result->body << std::endl;
        std::cout << "Expected output: " << b << std::endl;
        assert(false);
    }
}
    
void check(Expr a, Expr b, int lo = 0, int hi = 10) {
    // Use two For loops to set intervals on the variables.
    Stmt storea = Store::make("buf", a, Expr(0));
    Stmt storeb = Store::make("buf", b, Expr(0));
    check(storea, storeb, lo, hi);
}

}

void bounds_simplify_test() {
    Var x("x");
    vector<Expr> input_site_1 = vec(clamp(x,0,10));
    vector<Expr> input_site_2 = vec(clamp(x+1,0,10));
    vector<Expr> input_site_1_simplified = vec(min(x,10));
    vector<Expr> input_site_2_simplified = vec(min(x+1,10));
    vector<Expr> output_site = vec(x+1);

    Stmt loop = For::make("x", 3, 10 /* 3 to 12 inclusive */, For::Serial,  
                        Provide::make("output", 
                                    Add::make(
                                        Call::make(Int(32), "input", input_site_1),
                                        Call::make(Int(32), "input", input_site_2)),
                                    output_site));
    Stmt result = For::make("x", 3, 10, For::Serial,  
                        Provide::make("output", 
                                    Add::make(
                                        Call::make(Int(32), "input", input_site_1_simplified),
                                        Call::make(Int(32), "input", input_site_2_simplified)),
                                    output_site));

                                    check(Select::make(x < 11, x*2, x*3), x*2);

	// check sets x to be a For loop variable on the range 0 to 10 inclusive
    check(Min::make(x, 9), Min::make(x, 9));
    check(Min::make(x, 10), x);
    check(clamp(x, 1, 5), clamp(x, 1, 5));
    check(clamp(x, -1, 15), x);
    check(clamp(x-1, -1, 9), x-1);
    check(Clamp::make(Clamp::Wrap, x, 0, 10), x);
    check(Clamp::make(Clamp::None, x), x);
	//check(Clamp::make(Clamp::Reflect, x+1, 0, 10), x, 10, 10); // Cannot optimise this until it is lowered
    check(abs(min(x,10)), Call::make(Int(32), "abs_i32", vec(Expr(x))));
    check(abs(Call::make(Int(16), "input", input_site_1)),
        abs(Call::make(Int(16), "input", vec(Expr(x)))));
    check(abs(cast(Int(16), Call::make(UInt(8), "input", input_site_1))),
        abs(cast(Int(16), Call::make(UInt(8), "input", vec(Expr(x))))));
    
    check(loop, result);
    
    std::cout << "Simplify bounds test passed" << std::endl;
}

}
}
