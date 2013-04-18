#include "LoopPartition.h"
#include "IntervalAnalysis.h"
#include "IR.h"
#include "InlineLet.h"
#include "IROperator.h"
#include "IRPrinter.h"
#include "IRRewriter.h"
#include "Scope.h"
#include "Log.h"
#include "Substitute.h"
#include "Solver.h"
#include "Simplify.h"

namespace Halide {
namespace Internal {

using std::string;
using std::map;

// Insert nodes for the solver.
// For loops define target variables.
// Comparisons, min, max, mod are all targets for loop partition optimisation.
// Let statements are aggressively inlined.
class LoopPreSolver : public InlineLet {
    std::vector<std::string> varlist;

    using IRMutator::visit;
    
    bool is_constant_expr(Expr a) { return Halide::Internal::is_constant_expr(varlist, a); }
    
    virtual void visit(const For *op) {
        // Indicate that the For variable is not a constant variable
        varlist.push_back(op->name);
        
        // Mutate the children including inlining of Let expressions.
        InlineLet::visit(op); 
        
        // Use the mutated for loop returned by InlineLet::visit(op)
        // Tricky pointer management issue arises here:
        // If we write:
        //    const For *forloop = stmt.as<For>() 
        // this does not retain an intrusive pointer to the underlying For
        // loop node once stmt is reused.  The outcome is that
        //    stmt = new For(forloop, ...)
        // crashes because stmt is reinitialised and the For loop node is destroyed;
        // it turns out that the node is destroyed before the new node is constructed.
        // Assign stmt to result so that this does not happen.
        Stmt result = stmt;
        const For *forloop = result.as<For>();
        assert(forloop && "InlineLet did not return a For loop");
        
        // Construct a new body that wraps the loop body as a target variable for solver.
        // The source node is the original op.
        Stmt body = new StmtTargetVar(op->name, forloop->body, op);
        varlist.pop_back();
        stmt = new For(forloop, forloop->min, forloop->extent, body);
    }
    
    // Min can be eliminated in a loop body if the target expression is always either
    // <= the limit or >= the limit.
    virtual void visit(const Min *op) {
        // Min(a,b).
        // Min(a,k) or Min(k,a): Solve for a on (-infinity,k).
        // Min(a,b) is not solved because it is too general, although we could
        // solve Min(a-b,0)+b.
        Expr a = mutate(op->a);
        Expr b = mutate(op->b);
        if (is_constant_expr(a) && ! is_constant_expr(b)) std::swap(a,b);
        if (is_constant_expr(b)) {
            expr = new Min(new Solve(a, Interval(new Infinity(-1), b)), b);
        } else if (a.same_as(op->a) && b.same_as(op->b)) {
            expr = op;
        } else {
            expr = new Min(a, b);
        }
    }

    // Max can be eliminated in a loop body if the target expression is always either
    // <= the limit or >= the limit.
    virtual void visit(const Max *op) {
        // Max(a,b).
        // Max(a,k) or Max(k,a): Solve for a on (k, infinity)
        // Max(a,b) is not solved because it is too general, although we could
        // solve Max(a-b,0)+b.
        Expr a = mutate(op->a);
        Expr b = mutate(op->b);
        if (is_constant_expr(a) && ! is_constant_expr(b)) std::swap(a,b);
        if (is_constant_expr(b)) {
            expr = new Max(new Solve(a, Interval(b, new Infinity(+1))), b);
        } else if (a.same_as(op->a) && b.same_as(op->b)) {
            expr = op;
        } else {
            expr = new Max(a, b);
        }
    }
    
    // All comparisons should first be simplified to LT or EQ.
    // LT can be eliminated in a loop body if it always evaluated either to true or to false.
    virtual void visit(const LT *op) {
        Expr a = mutate(op->a);
        Expr b = mutate(op->b);
        if (is_constant_expr(a)) {
            // ka < b: Solve for b on (ka+1,infinity) [integer types] or (ka,infinity) [float]
            Expr limit = a;
            if (b.type().is_int() || b.type().is_uint()) {
                limit = simplify(limit + make_one(b.type()));
            }
            expr = new LT(a, new Solve(b, Interval(limit, new Infinity(+1))));
        } else if (is_constant_expr(b)) {
            // a < kb: Solve for a on (-infinity, kb-1) or (-infinity,kb)
            Expr limit = b;
            if (a.type().is_int() || a.type().is_uint()) {
                limit = simplify(limit - make_one(a.type()));
            }
            expr = new LT(new Solve(a, Interval(new Infinity(-1), limit)), b);
        } else if (a.same_as(op->a) && b.same_as(op->b)) {
            expr = op;
        } else {
            expr = new LT(a, b);
        }
    }
    
    // Do not solve for equality/inequality because it does not generate a useful
    // loop partition (a single case is isolated).  Note that, for small loops,
    // loop unrolling may make each case individual.
    
    // Mod can be eliminated in a loop body if the expression passed to Mod is
    // known to be in the range of the Mod.
    virtual void visit(const Mod *op) {
        Expr a = mutate(op->a);
        Expr b = mutate(op->b);
        // Mod is handled for constant modulus.  A constant variable
        // expression cannot be handled unless we know for sure whether it is
        // positive or negative.
        if (is_positive_const(b)) {
            // a % kb: Solve for a on (0,kb-1) or (0,kb)
            Expr limit = b;
            if (a.type().is_int() || a.type().is_uint()) {
                limit = simplify(limit - make_one(a.type()));
            }
            expr = new Mod(new Solve(a, Interval(make_zero(a.type()), limit)), b);
        } else if (is_negative_const(b)) {
            // a % kb: Solve for a on (kb+1,0) or (kb,0)
            Expr limit = b;
            if (a.type().is_int() || a.type().is_uint()) {
                limit = simplify(limit + make_one(a.type()));
            }
            expr = new Mod(new Solve(a, Interval(limit, make_zero(a.type()))), b);
        } else if (a.same_as(op->a) && b.same_as(op->b)) {
            expr = op;
        } else {
            expr = new Mod(a, b);
        }
    }
    
    // Clamp can be eliminated in a loop body if the expression is always in range
    virtual void visit(const Clamp *op) {
        Expr a = mutate(op->a);
        Expr min = mutate(op->min);
        Expr max = mutate(op->max);
        Expr p1 = mutate(op->p1);
        
        if (is_constant_expr(min) && is_constant_expr(max)) {
            // a on (min, max)
            expr = new Clamp(op->clamptype, new Solve(a, Interval(min, max)), min, max, p1);
        } else if (a.same_as(op->a) && min.same_as(op->min) && 
                   max.same_as(op->max) && p1.same_as(op->p1)) {
            expr = op;
        } else {
            expr = new Clamp(op->clamptype, a, min, max, p1);
        }
    }
};


// Perform loop partition optimisation for all For loops
class LoopPartition : public IRMutator {
    using IRMutator::visit;
public:
    Stmt solved;

protected:
    std::vector<int> partition_points(std::vector<Solution> sol, Expr endpoint, bool negate) {
        std::vector<int> results;
        
        for (size_t i = 0; i < sol.size(); i++) {
            for (size_t j = 0; j < sol[i].intervals.size(); j++) {
                // min means decision is < min vs >= min
                // partition value of p means take first p elements out, and that
                // corresponds to < p vs >= p
                Expr diff = simplify(sol[i].intervals[j].min - endpoint);
                std::cout << diff << "\n";
                int ival;
                if (get_const_int(diff, ival)) {
                    if (negate) ival = -ival;
                    if (ival > 0) results.push_back(ival);
                }
                // max means decision is <= max vs > max
                diff = simplify(sol[i].intervals[j].max - endpoint + 1);
                std::cout << diff << "\n";
                if (get_const_int(diff, ival)) {
                    if (negate) ival = -ival;
                    if (ival > 0) results.push_back(ival);
                }
            }
        }
        return results;
    }
    
    void visit(const For *op) {
        bool done = false;
        
        // Only apply optimisation to serial For loops.
        // Parallel loops may also be eligible for optimisation, but not yet handled.
        // Vectorised loops are not eligible, and unrolled loops should be fully
        // optimised for each iteration due to the unrolling.
        Stmt new_body = mutate(op->body);
        if (op->for_type == For::Serial || op->for_type == For::Parallel) {
            // Manually specified loop partitioning.
            if (op->partition_begin > 0 || op->partition_end > 0) {
                //log(0) << "Found serial for loop \n" << Stmt(op) << "\n";
                // Greedy allocation of up to partition_begin loop iterations to before.
                Expr before_min = op->min;
                Expr before_extent = min(op->partition_begin, op->extent);
                // Greedy allocation of up to partition_end loop iterations to after.
                Expr main_min = before_min + op->partition_begin; // May be BIG
                Expr main_extent = op->extent - op->partition_begin - op->partition_end; // May be negative
                Expr after_min = main_min + max(0,main_extent);
                Expr after_extent = min(op->partition_end, op->extent - before_extent);
                // Now generate the partitioned loops.
                // Mark them by using negative numbers for the partition information.
                Stmt before = new For(op->name, before_min, before_extent, op->for_type, -2, -2, op->body);
                Stmt main = new For(op->name, main_min, main_extent, op->for_type, -3, -3, new_body);
                Stmt after = new For(op->name, after_min, after_extent, op->for_type, -4, -4, op->body);
                stmt = new Block(new Block(before,main),after);
                if (stmt.same_as(op)) {
                    stmt = op;
                }
                done = true;
            } else {
                // Automatic loop partitioning.
                // Search for solutions related to this particular for loop.
                std::vector<Solution> solutions = extract_solutions(op->name, op, solved);
                std::cout << "For loop: \n" << Stmt(op);
                std::cout << "Solutions: \n";
                for (size_t i = 0; i < solutions.size(); i++) {
                    std::cout << solutions[i].var << " " << solutions[i].intervals << "\n";
                }
                // Compute the partition points relative to each end of the loop
                // and simplify them.  The meaning of a partition point value is to
                // partition the loop at that number of elements from the start or end.
                // Negative values and zero are discarded as useless.
                // Decision points represent < limit vs >= limit, so min + extent is used
                // as the loop end value.
                std::vector<int> begin = partition_points(solutions, op->min, false);
                std::vector<int> end = partition_points(solutions, op->min + op->extent, true);
                std::cout << "Partition begin: ";
                for (size_t i = 0; i < begin.size(); i++) {
                    std::cout << begin[i] << " ";
                }
                std::cout << std::endl << "Partition end: ";
                for (size_t i = 0; i < end.size(); i++) {
                    std::cout << end[i] << " ";
                }
                std::cout << std::endl;
                
                // Now, it gets tricky.
                // If the loop bounds are constants
            }
        }
        if (! done) {
            if (new_body.same_as(op->body)) {
                stmt = op;
            } else {
                stmt = new For(op, op->min, op->extent, new_body);
            }
        }
    }
public:
    LoopPartition() {}

};

Stmt loop_partition(Stmt s) {
    return LoopPartition().mutate(s);
}

// Test loop partition routines.

// Test pre processing.

# if 1
void test_loop_partition_1() {
    Type i32 = Int(32);
    Expr x = new Variable(Int(32), "x");
    Expr y = new Variable(Int(32), "y");

    Expr input = new Call(Int(16), "input", vec((x - 10) % 100 + 10));
    Expr select = new Select(x > 3, new Select(x < 87, input, new Cast(Int(16), -17)),
                             new Cast(Int(16), -17));
    Stmt store = new Store("buf", select, x - 1);
    Stmt for_loop = new For("x", 0, 100, For::Parallel, 0, 0, store);
    Expr call = new Call(i32, "buf", vec(max(min(x,100),0)));
    Expr call2 = new Call(i32, "buf", vec(max(min(x-1,100),0)));
    Expr call3 = new Call(i32, "buf", vec(Expr(new Clamp(Clamp::Reflect, x+1, 0, 100))));
    Stmt store2 = new Store("out", call + call2 + call3 + 1, x);
    Stmt for_loop2 = new For("x", 0, 100, For::Serial , 0, 0, store2);
    Stmt pipeline = new Pipeline("buf", for_loop, Stmt(), for_loop2);
    
    std::cout << "Raw:\n" << pipeline << "\n";
    Stmt simp = simplify(pipeline);
    std::cout << "Simplified:\n" << simp << "\n";
    Stmt pre = LoopPreSolver().mutate(simp);
    std::cout << "LoopPreSolver:\n" << pre << "\n";
    Stmt solved = solver(pre);
    std::cout << "Solved:\n" << solved << "\n";
    std::vector<Solution> solutions = extract_solutions("x", Stmt(), solved);
    for (size_t i = 0; i < solutions.size(); i++) {
        std::cout << solutions[i].var << " " << solutions[i].intervals << "\n";
    }
    
    LoopPartition loop_part;
    loop_part.solved = solved;
    Stmt part = loop_part.mutate(simp);
    std::cout << "Partitioned:\n" << part << "\n";
}
# endif

void loop_partition_test() {
    test_loop_partition_1();
    
    std::cout << "Loop Partition test passed\n";
}

}
}
