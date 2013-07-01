#include "BoundsSimplify.h"
#include "BoundsAnalysis.h"
#include "CodeLogger.h"
#include "IRCacheMutator.h"
#include "IR.h"
#include "IROperator.h"
#include "IREquality.h"
#include "IRPrinter.h"
#include "DomInterval.h"
#include "Util.h"
#include "Var.h"
#include "Log.h"
#include <iostream>
#include <vector>

using std::vector;

# define LOGLEVEL 4

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
        DomInterval bounds_a = bounds.bounds(op->a);
        DomInterval bounds_b = bounds.bounds(op->b);
        code_logger.log() << "BoundsSimplify Mod: " << Expr(op) << "\n";
        code_logger.log() << "    Interval a " << bounds_a << "\n";
        code_logger.log() << "    Interval b " << bounds_b << "\n";
        if (proved(bounds_b.min > bounds_a.max) && proved(bounds_a.min >= 0)) {
            // The expression a is always in the bounds of the (positive) modulus b.
            code_logger.log() << "Proved " << (bounds_b.min > bounds_a.max) << "\n";
            code_logger.log() << "    and +ve modulus proof " << (bounds_a.min >= 0) << "\n";
            expr = mutate(op->a);
        } else if (proved(bounds_b.max < bounds_a.min) && proved(bounds_a.max <= 0)) {
            // The expression a is always in the bounds of the negative modulus b.
            code_logger.log() << "Proved " << (bounds_b.max < bounds_a.min) << "\n";
            code_logger.log() << "   and -ve modulus proof " << (bounds_a.max <= 0) << "\n";
            expr = mutate(op->a);
        } else {
            code_logger.log() << "Could not bounds simplify Mod\n";
            Super::visit(op);
        }
        code_logger.log() << "BoundsSimplify Mod result: " << expr << "\n";
    }

	//LH
    void visit(const Clamp *op) {
        DomInterval bounds_a = bounds.bounds(op->a);
        DomInterval bounds_min = bounds.bounds(op->min);
        DomInterval bounds_max = bounds.bounds(op->max);
        code_logger.log() << "BoundsSimplify Clamp: " << Expr(op) << "\n";
        code_logger.log() << "    Interval a " << bounds_a << "\n";
        code_logger.log() << "    Interval min " << bounds_min << "\n";
        code_logger.log() << "    Interval max " << bounds_max << "\n";
        if (op->clamptype == Clamp::None) {
            code_logger.log() << "Clamp None is trivially removed\n";
            expr = mutate(op->a);
        } else if (proved(bounds_min.max <= bounds_a.min) && 
            proved(bounds_max.min >= bounds_a.max)) {
            code_logger.log() << "Proved " << (bounds_min.max <= bounds_a.min) << "\n";
            code_logger.log() << "   and " << (bounds_max.min >= bounds_a.max) << "\n";
            // The expression is always in bounds, so the clamp is not required at all.
            expr = mutate(op->a);
        } else {
            code_logger.log() << "Could not bounds simplify Clamp\n";
            Super::visit(op);
        }
        code_logger.log() << "BoundsSimplify Clamp result: " << expr << "\n";
    }

    void visit(const Min *op) {
        DomInterval bounds_a = bounds.bounds(op->a);
        DomInterval bounds_b = bounds.bounds(op->b);
        code_logger.log() << "BoundsSimplify Min: " << Expr(op) << "\n";
        code_logger.log() << "    Interval a " << bounds_a << "\n";
        code_logger.log() << "    Interval b " << bounds_b << "\n";
        if (proved(bounds_a.max <= bounds_b.min)) {
            code_logger.log() << "Proved " << (bounds_a.max <= bounds_b.min) << "\n";
            expr = mutate(op->a);
        } else if (proved(bounds_b.max <= bounds_a.min)) {
            code_logger.log() << "Proved " << (bounds_b.max <= bounds_a.min) << "\n";
            expr = mutate(op->b);
        } else {
            code_logger.log() << "Could not bounds simplify Min\n";
            Super::visit(op);
        }
        code_logger.log() << "BoundsSimplify Min result: " << expr << "\n";
    }

    void visit(const Max *op) {
        DomInterval bounds_a = bounds.bounds(op->a);
        DomInterval bounds_b = bounds.bounds(op->b);
        code_logger.log() << "BoundsSimplify Max: " << Expr(op) << "\n";
        code_logger.log() << "    Interval a " << bounds_a << "\n";
        code_logger.log() << "    Interval b " << bounds_b << "\n";
        if (proved(bounds_a.min >= bounds_b.max)) {
            code_logger.log() << "Proved " << (bounds_a.min >= bounds_b.max) << "\n";
            expr = mutate(op->a);
        } else if (proved(bounds_b.min >= bounds_a.max)) {
            code_logger.log() << "Proved " << (bounds_b.min >= bounds_a.max) << "\n";
            expr = mutate(op->b);
        } else {
            code_logger.log() << "Could not bounds simplify Max\n";
            code_logger.log() << "Could not prove: " << (bounds_a.min >= bounds_b.max) << "\n";
            code_logger.log() << "            nor: " << (bounds_b.min >= bounds_a.max) << "\n";
            Super::visit(op);
        }
        code_logger.log() << "BoundsSimplify Max result: " << Expr(op) << " --> " << expr << "\n";
    }
    
    // This code may not do much. Mind you, it could be used to simplify
    // conditional expressions, but you also need to handle other conditionals.
    // This code IS useful for debugging when a conditional is not solved by Select.
    void visit(const LT *op) {
        DomInterval bounds_a = bounds.bounds(op->a);
        DomInterval bounds_b = bounds.bounds(op->b);
        code_logger.log() << "BoundsSimplify LT: " << Expr(op) << "\n";
        code_logger.log() << "    Interval a " << bounds_a << "\n";
        code_logger.log() << "    Interval b " << bounds_b << "\n";
        if (proved(bounds_a.min >= bounds_b.max)) {
            code_logger.log() << "Proved " << (bounds_a.min >= bounds_b.max) << "\n";
            expr = const_false(op->type.width);
        } else if (proved(bounds_a.max < bounds_b.min)) {
            code_logger.log() << "Proved " << (bounds_a.max < bounds_b.min) << "\n";
            expr = const_true(op->type.width);
        } else {
            code_logger.log() << "Could not bounds simplify LT\n";
            Super::visit(op);
        }
        code_logger.log() << "BoundsSimplify LT result: " << expr << "\n";
    }

    void visit(const Select *op) {
        // Note: Interval analysis on the condition expression can implicitly
        // perform bounds-based optimisation of the condition expression itself.
        // That is because bounds analysis computes bounds intervals on the
        // two sides of the condition, and then tries to prove them true or false.
        DomInterval bounds_cond = bounds.bounds(op->condition);
        code_logger.log() << "BoundsSimplify Select: " << Expr(op) << "\n";
        code_logger.log() << "    Interval cond " << bounds_cond << "\n";
        if (is_one(bounds_cond.min)) {
            // Provably always true condition.
            code_logger.log() << "Proved always true\n";
            expr = mutate(op->true_value);
        } else if (is_zero(bounds_cond.max)) {
            code_logger.log() << "Proved always false\n";
            expr = mutate(op->false_value);
        } else {
            code_logger.log() << "Could not bounds simplify Select\n";
            Super::visit(op);
        }
        code_logger.log() << "BoundsSimplify Select result: " << expr << "\n";
    }
          
# if 0
    void visit(const AssertStmt *op) {
        // Complex bounds can arise in assertions. Skip them.
        stmt = op;
        return;
    }
# endif

# if 1
    // For loop visit only to focus debugging.
    void visit(const For *op) {
        int old_debug_level = log::debug_level;
        
        // If it is the main loop, keep debug level, otherwise kill debug.
        code_logger.log() << "------- Begin loop " << op->name << " " << op->loop_split << "\n";
        enter(op->body);
        code_logger.log() << "    interval " << op->name << ": " << bounds.bounds(new Variable(Int(32), op->name)) << "\n";
        leave(op->body);
        if (op->loop_split.status != LoopSplitInfo::Main && 
            op->loop_split.status != LoopSplitInfo::Ordinary) {
            log::debug_level = -1;
            if (log::debug_level != old_debug_level) 
                log(LOGLEVEL-1) << "--- Debug level " << old_debug_level << " -> " << log::debug_level << "\n";
        }
        Super::visit(op);
        
        code_logger.log() << "-------- End loop " << op->name << "\n";
        if (log::debug_level != old_debug_level) 
            log(LOGLEVEL-1) << "--- Debug level " << log::debug_level << " -> " << old_debug_level << "\n";
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
void check(Stmt a, Stmt b) {
    // Use loop to set intervals on the variables.
    Stmt fora = new For("x", Expr(0), Expr(11), For::Serial, a);
    Stmt forb = new For("x", Expr(0), Expr(11), For::Serial, b);
    // Simplify a.
    Stmt simpler = bounds_simplify(fora);
    const For *result = simpler.as<For>();
    if (!equal(result->body, b)) {
        std::cout << std::endl << "Simplify bounds failure: " << std::endl;
        std::cout << "Input: " << a << std::endl;
        std::cout << "Output: " << result->body << std::endl;
        std::cout << "Expected output: " << b << std::endl;
        assert(false);
    }
}
    
void check(Expr a, Expr b) {
    // Use two For loops, one with an undefined upper bound (!) to set intervals on the variables.
    Stmt storea = new Store("x", a, Expr(0));
    Stmt storeb = new Store("x", b, Expr(0));
    check(storea, storeb);
}

}

void bounds_simplify_test() {
    Var x("x");
    vector<Expr> input_site_1 = vec(clamp(x,0,10));
    vector<Expr> input_site_2 = vec(clamp(x+1,0,10));
    vector<Expr> input_site_1_simplified = vec(min(x,10));
    vector<Expr> input_site_2_simplified = vec(min(x+1,10));
    vector<Expr> output_site = vec(x+1);

    Stmt loop = new For("x", 3, 10 /* 3 to 12 inclusive */, For::Serial,  
                        new Provide("output", 
                                    new Add(
                                        new Call(Int(32), "input", input_site_1),
                                        new Call(Int(32), "input", input_site_2)),
                                    output_site));
    Stmt result = new For("x", 3, 10, For::Serial,  
                        new Provide("output", 
                                    new Add(
                                        new Call(Int(32), "input", input_site_1_simplified),
                                        new Call(Int(32), "input", input_site_2_simplified)),
                                    output_site));

                                    check(new Select(x < 11, x*2, x*3), x*2);

    check(new Min(x, 9), new Min(x, 9));
    check(new Min(x, 10), x);
    check(clamp(x, 1, 5), clamp(x, 1, 5));
    check(clamp(x, -1, 15), x);
    check(clamp(x-1, -1, 9), x-1);
    check(new Clamp(Clamp::Wrap, x, 0, 10), x);
    check(new Clamp(Clamp::None, x), x);
    check(abs(min(x,10)), new Call(Int(32), "abs_i32", vec(Expr(x))));
    check(abs(new Call(Int(16), "input", input_site_1)),
        abs(new Call(Int(16), "input", vec(Expr(x)))));
    check(abs(cast(Int(16), new Call(UInt(8), "input", input_site_1))),
        abs(cast(Int(16), new Call(UInt(8), "input", vec(Expr(x))))));
    
    check(loop, result);
    
    std::cout << "Simplify bounds test passed" << std::endl;
}

}
}
