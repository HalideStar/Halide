#include "LoopPartition.h"
#include "IntervalAnalysis.h"
#include "IR.h"
#include "InlineLet.h"
#include "IREquality.h"
#include "IROperator.h"
#include "IRPrinter.h"
#include "IRRewriter.h"
#include "Scope.h"
#include "Log.h"
#include "Substitute.h"
#include "Solver.h"
#include "Simplify.h"
#include "Options.h"
#include "CodeLogger.h"

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
        // Only serial and parallel for loops can be partitioned.
        // Vector and Unrolled for loops, if present, become intervals for the solver to deal with.
        if (op->for_type == For::Serial || op->for_type == For::Parallel) {
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
        } else {
# if 0
            // It is a real horrible hack but for now we try
            // converting a vector or unrolled for loop into
            // a LetStmt with a clamp representing the range of values.
            // The solver should really be able to solve such an expression.
            Stmt body = op->body;
            Expr unknownvar = new Variable(op->min.type(), "unknown." + op->name);
            Expr value = clamp(unknownvar, op->min, op->min + (op->extent-1));
            Stmt letstmt = new LetStmt(op->name, value, body);
            mutate(letstmt);
# endif
            // Treat the unrolled or vectorised loop as a constant - it will be
            // an interval when solved.
            // Mutate the children including inlining of Let expressions.
            InlineLet::visit(op); 
        }
    }
    
    // Solve the non-constant side of a min expression.
    // If we can partition the loop so that one side is always <= or >= the other side
    // then min can be eliminated.
    virtual void visit(const Min *op) {
        // Min(a,b).
        // Min(a,k) or Min(k,a): Solve for a on (-infinity,k).
        // Min(a,b) is not solved because it is too general.
        Expr a = mutate(op->a);
        Expr b = mutate(op->b);
        if (is_constant_expr(a) && ! is_constant_expr(b)) std::swap(a,b);
        if (!is_constant_expr(a) && is_constant_expr(b)) {
            expr = new Min(new Solve(a, Ival(make_infinity(b.type(), -1), b)), b);
        } else if (a.same_as(op->a) && b.same_as(op->b)) {
            expr = op;
        } else {
            expr = new Min(a, b);
        }
    }

    // Solve the non-constant side of a max expression.
    virtual void visit(const Max *op) {
        // Max(a,b).
        // Max(a,k) or Max(k,a): Solve for a on (k, infinity)
        // Max(a,b) is not solved because it is too general.
        Expr a = mutate(op->a);
        Expr b = mutate(op->b);
        if (is_constant_expr(a) && ! is_constant_expr(b)) std::swap(a,b);
        if (!is_constant_expr(a) && is_constant_expr(b)) {
            expr = new Max(new Solve(a, Ival(b, make_infinity(b.type(), +1))), b);
        } else if (a.same_as(op->a) && b.same_as(op->b)) {
            expr = op;
        } else {
            expr = new Max(a, b);
        }
    }
    
    // Solve an inequality.
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
            expr = new LT(a, new Solve(b, Ival(limit, make_infinity(b.type(), +1))));
        } else if (is_constant_expr(b)) {
            // a < kb: Solve for a on (-infinity, kb-1) or (-infinity,kb)
            Expr limit = b;
            if (a.type().is_int() || a.type().is_uint()) {
                limit = simplify(limit - make_one(a.type()));
            }
            expr = new LT(new Solve(a, Ival(make_infinity(a.type(), -1), limit)), b);
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
            expr = new Mod(new Solve(a, Ival(make_zero(a.type()), limit)), b);
        } else if (is_negative_const(b)) {
            // a % kb: Solve for a on (kb+1,0) or (kb,0)
            Expr limit = b;
            if (a.type().is_int() || a.type().is_uint()) {
                limit = simplify(limit + make_one(a.type()));
            }
            expr = new Mod(new Solve(a, Ival(limit, make_zero(a.type()))), b);
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
            expr = new Clamp(op->clamptype, new Solve(a, Ival(min, max)), min, max, p1);
        } else if (a.same_as(op->a) && min.same_as(op->min) && 
                   max.same_as(op->max) && p1.same_as(op->p1)) {
            expr = op;
        } else {
            expr = new Clamp(op->clamptype, a, min, max, p1);
        }
    }
};


// HasVariableMatch walks an argument expression and determines
// whether the expression contains any variable that matches the pattern.
// The pattern is a string that should be found in the variable - that is all
// there is to it.

class HasVariableMatch : public IRVisitor {
private:
    const std::string &pattern;

public:
    bool result;
    HasVariableMatch(const std::string &_pattern) : pattern(_pattern), result(false) {}
    
private:
    using IRVisitor::visit;
    
    void visit(const Variable *op) {
        if (result) return; // Once one is found, no need to find more.
        // Check whether variable name is in the list of known names.
        result = op->name.find(pattern) != std::string::npos;
    }

    // If a Let node defines one of the variables that we are looking for,
    // then the variable inside the Let is not the same variable.
    void visit(const Let *op) {
        if (result) return; // Do not continue checking once one variable is found.
        
        // If the value expression of the Let contains a matching, then
        // we have found the variable.
        op->value.accept(this);
        
        // To do this properly, we would have to track Let bindings, but our
        // application is unlikely to encounter any actual Let.  So, if the
        // Let binding name is a match then refuse to visit the body.
        // This means that we may say "no" when we should say "yes" but never
        // "yes" when we should say "no".
        if (op->name.find(pattern) != std::string::npos) {
            // Skip it.  Otherwise, we would have to track exclusions.
        } else {
            op->body.accept(this);
        }
    }
};

bool has_variable_match(std::string pattern, Expr e) {
    HasVariableMatch matcher(pattern);
    e.accept(&matcher);
    return matcher.result;
}

// Perform loop partition optimisation on For loops
class LoopPartition : public IRMutator {
    using IRMutator::visit;
public:
    Stmt solved;

protected:
    // Insert a partition point into a list.  Search to find a compatible expression and update that expression.
    // An expression is compatible if we can either prove that the new point is <= or > than the existing point.
    // For end points list (is_end true) replace expression with new point if new point <= expression.
    // For start points list (is_end false) replace expresison with new point if new point > expression.
    void insert_partition_point(Expr point, std::vector<Expr> &points, bool is_end) {
        bool disproved;
        size_t i;
        for (i = 0; i < points.size(); i++) {
            if (proved(point >  points[i], disproved)) {
                if (! is_end) points[i] = point;
                break; // Compatible expression found
            } else if (disproved /* proved(point <= points[i]) */) {
                if (is_end) points[i] = point;
                break; // Compatible expression found
            }
        }
        if (i >= points.size()) {
            // Not found
            points.push_back(point); // Add a new point expression
        }
        return;
    }
    
    // Insert a partition point into the appropriate list; but if numeric update num_start or num_end instead.
    void insert_partition_point(Expr point, int ithresh, int &num_start, int &num_end, std::vector<Expr> &starts, std::vector<Expr> &ends) {
        int ival;
        if (get_const_int(point, ival)) {
            // A numeric value.  
            if (ival < ithresh && ival > num_start) num_start = ival;
            else if (ival >= ithresh && ival < num_end) num_end = ival;
        } else if (has_variable_match(".min.", point)) {
            // Symbolic expression.
            if (has_variable_match(".extent.", point)) {
                // End point.
                insert_partition_point(point, ends, true);
            } else {
                // Start point.
                insert_partition_point(point, starts, false);
            }
        }
    }
    
    void partition_points(std::vector<Solution> sol, std::vector<Expr> &starts, std::vector<Expr> &ends) {
        starts.clear();
        ends.clear();
        
        // First pass, find the min and max values of any numerics that occur.
        int ival;
        int imin, imax;
        imin = Int(32).imax();
        imax = Int(32).imin();
        for (size_t i = 0; i < sol.size(); i++) {
            for (size_t j = 0; j < sol[i].intervals.size(); j++) {
                // min means decision is < min vs >= min
                Expr start = sol[i].intervals[j].min;
                // max means decision is <= max vs > max, so adjust for consistency
                Expr end = simplify(sol[i].intervals[j].max + 1);
                // Classify the points as numeric or not.
                if (get_const_int(start, ival)) {
                    // A numeric value.  
                    if (ival < imin) imin = ival;
                    if (ival > imax) imax = ival;
                }
                if (get_const_int(end, ival)) {
                    // A numeric value.  
                    if (ival < imin) imin = ival;
                    if (ival > imax) imax = ival;
                }
            }
        }
        int ithresh = (imin + imax) / 2;
        
        // Second pass: put the partition points into appropriate places.
        // The numeric start is the max of all numerics classified as start
        int num_start = Int(32).imin();
        int num_end = Int(32).imax();
        for (size_t i = 0; i < sol.size(); i++) {
            for (size_t j = 0; j < sol[i].intervals.size(); j++) {
                // min means decision is < min vs >= min
                Expr start = sol[i].intervals[j].min;
                // max means decision is <= max vs > max, so adjust for consistency
                Expr end = simplify(sol[i].intervals[j].max + 1);
                insert_partition_point(start, ithresh, num_start, num_end, starts, ends);
                insert_partition_point(end, ithresh, num_start, num_end, starts, ends);
            }
        }
        if (num_start > Int(32).imin()) starts.push_back(Expr(num_start));
        if (num_end < Int(32).imax()) ends.push_back(Expr(num_end));
        
        return;
    }
    
    void append_stmt(Stmt &block, Stmt s) {
        if (! block.defined()) block = s;
        else block = new Block(block, s);
    }
    
    void visit(const For *op) {
        bool done = false;
        
        // Only apply optimisation to serial For loops and Parallel For loops.
        // Vectorised loops are not eligible, and unrolled loops should be fully
        // optimised for each iteration due to the unrolling.
        Stmt new_body = mutate(op->body);
        if ((op->for_type == For::Serial || op->for_type == For::Parallel) &&
             (op->partition.status == PartitionInfo::Ordinary)) {
            Ival part;
            if (op->partition.interval_defined()) {
                part = op->partition.interval;
            } else if (op->partition.auto_partition == PartitionInfo::Yes || 
                      (op->partition.auto_partition == PartitionInfo::Undefined && global_options.loop_partition_all)) {
                // Automatic loop partitioning.  Determine an interval for the loop to be partitioned on.

                // Search for solutions related to this particular for loop.
                std::vector<Solution> solutions = extract_solutions(op->name, op, solved);
                //std::cout << global_options;
                std::cout << "For loop: \n" << Stmt(op);
                std::cout << "Solutions: \n";
                for (size_t i = 0; i < solutions.size(); i++) {
                    std::cout << solutions[i].var << " " << solutions[i].intervals << "\n";
                }
                
                // Compute loop partition points for each end of the loop.
                // If the information that we get is purely numeric, then we need to
                // guess what is the beginning and what is the end.
                // A reasonable guess is to take the average of the min and max,
                // and use that as a threshold.  An even better guess may be to find the
                // greatest jump in the sequence.
                // If the information is symbolic, then expressions involving .extent. variables
                // are end, and those involving .min. but not .extent. are beginning.
                
                std::vector<Expr> starts, ends;
                partition_points(solutions, starts, ends);
# if 1
                std::cout << "Partition start: ";
                for (size_t i = 0; i < starts.size(); i++) {
                    std::cout << starts[i] << " ";
                }
                std::cout << "\n" << "Partition end: ";
                for (size_t i = 0; i < ends.size(); i++) {
                    std::cout << ends[i] << " ";
                }
                std::cout << "\n";
# endif               
                // The interval for the main loop is max(starts) to min(ends)-1.
                // If no partition points are found then the interval has undefined min and/or max.
                Expr part_start, part_end;
                if (starts.size() > 0) {
                    part_start = starts[0];
                    for (size_t i = 1; i < starts.size(); i++) {
                        part_start = min (part_start, starts[i]);
                    }
                }
                if (ends.size() > 0) {
                    part_end = ends[0];
                    for (size_t i = 1; i < ends.size(); i++) {
                        part_end = min (part_end, ends[i]);
                    }
                    part_end = part_end - 1;
                }
                
                part = Ival(part_start, part_end);
                
                log(0) << "Auto partition: " << op->name << " " << part << "\n";
            }
            
            if ((part.min.defined() && infinity_count(part.min) == 0) || 
                (part.max.defined() && infinity_count(part.max) == 0)) {
                // The partitioned loops are as follows:
                // for (x, .min, Min(start - .min, .extent))
                //     This loop starts at .min and never reaches start.  It also never exceeds .extent.
                // for (x, Max(start,.min), Min(end,.min+.extent)-Max(start,.min))
                //     This loop starts at start or .min, whichever is greater.
                //     It never reaches end.
                // for (x, end, .min+.extent - end)
                //     This loop starts at end.  It never reaches .min + .extent.
                // This may produce negative extent for one or more loops; a negative extent is not executed.
                //
                // The loop bounds being strange may produce out of bounds errors if this optimisation is
                // performed before bounds analysis, but why would anyone do that?
                //
                // For optimisation to be effective, it must be possible to determine from the loop bounds
                // that certain expressions are true or false.  This applies particularly to the main loop
                // and that is why we would rather have a negative extent then force it to be zero extent.
                //
                // A negative extent can cause problems for parallel for loops unless
                // halide_do_par_for returns immediately when size <= 0.
                string startName = op->name + ".start";
                string endName = op->name + ".end";
                Expr start, startValue, before_min, before_extent;
                if (part.min.defined() && infinity_count(part.min) == 0) {
                    start = new Variable(op->min.type(), startName);
                    startValue = simplify(part.min);
                    before_min = op->min;
                    //before_extent = max(min(start - op->min, op->extent), 0);
                    before_extent = min(start - op->min, op->extent);
                }
                Expr end, endValue, after_min, after_extent;
                if (part.max.defined() && infinity_count(part.max) == 0) {
                    end = new Variable(op->min.type(), endName);
                    endValue = simplify(part.max + 1); // end is after the end of the loop.
                    after_min = end;
                    //after_extent = max(op->min + op->extent - end, 0);
                    after_extent = op->min + op->extent - end;
                }
                Expr main_min, main_extent;
                if (start.defined()) {
                    main_min = max(start, op->min);
                } else {
                    main_min = op->min;
                }
                if (end.defined()) {
                    if (start.defined()) main_extent = min(end, op->min + op->extent) - max(start, op->min);
                    else main_extent = min(end, op->min + op->extent) - op->min;
                    //if (start.defined()) main_extent = min(max(end, start), op->min + op->extent) - max(start, op->min);
                    //else main_extent = min(max(end, op->min), op->min + op->extent) - op->min;
                } else {
                    if (start.defined()) main_extent = op->min + op->extent - max(start, op->min);
                    else main_extent = op->extent;
                }
                // Use Let binding for the main loop bounds, so no expressions there.
                Expr main_min_let, main_extent_let;
                string main_min_name, main_extent_name;
                if (global_options.loop_partition_letbind) {
                    main_min_let = main_min;
                    main_extent_let = main_extent;
                    main_min_name = op->name + ".mainmin";
                    main_extent_name = op->name + ".mainextent"; // Prefer not to use .extent to avoid confusion.
                    main_min = new Variable(op->min.type(), main_min_name);
                    main_extent = new Variable(op->min.type(), main_extent_name);
                }
                // Build the code.
                Stmt block; // An undefined block of code.
                // The before and after loops use the original loop body - their nested loops
                // are not partitioned.
                PartitionInfo p = op->partition;
                if (start.defined()) {
                    p.status = PartitionInfo::Before;
                    append_stmt(block, new For(op->name, before_min, before_extent, op->for_type, p, op->body));
                }
                p.status = PartitionInfo::Main;
                append_stmt(block, new For(op->name, main_min, main_extent, op->for_type, p, new_body));
                p.status = PartitionInfo::After;
                if (end.defined()) {
                    append_stmt(block, new For(op->name, after_min, after_extent, op->for_type, p, op->body));
                }
                if (global_options.loop_partition_letbind) {
                    block = new LetStmt(main_extent_name, main_extent_let, block);
                    block = new LetStmt(main_min_name, main_min_let, block);
                }
                if (end.defined()) {
                    block = new LetStmt(endName, endValue, block);
                }
                if (start.defined()) {
                    block = new LetStmt(startName, startValue, block);
                }
                stmt = block;
                if (equal(block, op)) { // Equality test required because new For loop is always constructed.
                    stmt = op;
                }
                //log(0) << "\nOriginal loop:\n";
                //log(0) << Stmt(op);
                //log(0) << "\nPartitioned loop:\n";
                //log(0) << stmt;
                done = true;
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
    //return LoopPartition().mutate(s);
    LoopPartition loop_part;
    s = simplify(s); // Must be fully simplified first
    code_logger.log(s, "simplify");
    Stmt pre = LoopPreSolver().mutate(s);
    code_logger.log(pre, "pre_solver");
    Stmt solved = solver(pre);
    code_logger.log(solved, "solved");
    loop_part.solved = solved;
    s = loop_part.mutate(s);
    code_logger.log(s, "loop_partition");
    return s;
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
    PartitionInfo partition(true);
    Stmt for_loop = new For("x", 0, 100, For::Parallel, partition, store);
    Expr call = new Call(i32, "buf", vec(max(min(x,100),0)));
    Expr call2 = new Call(i32, "buf", vec(max(min(x-1,100),0)));
    Expr call3 = new Call(i32, "buf", vec(Expr(new Clamp(Clamp::Reflect, x+1, 0, 100))));
    Stmt store2 = new Store("out", call + call2 + call3 + 1, x);
    PartitionInfo partition2(Ival(1,99));
    Stmt for_loop2 = new For("x", 0, 100, For::Serial, partition2, store2);
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
