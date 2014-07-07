#include "LoopPartition.h"
#include "BoundsAnalysis.h"
#include "IR.h"
#include "InlineLet.h"
#include "IREquality.h"
#include "IROperator.h"
#include "IRPrinter.h"
#include "Scope.h"
#include "Log.h"
#include "Substitute.h"
#include "Solver.h"
#include "Simplify.h"
#include "Options.h"
#include "CodeLogger.h"

# define LOGLEVEL 4
// LOOP_SPLIT_CONDITIONAL: If true, conditional expressions are used to derive loop split points.
# define LOOP_SPLIT_CONDITIONAL 0

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

    using InlineLet::visit;
    
    bool is_constant_expr(Expr a) { return Halide::Internal::is_constant_expr(varlist, a); }
    
    BoundsAnalysis bounds;
    
    virtual void visit(const For *op) {
        // Only serial and parallel for loops can be partitioned.
        // Vector and Unrolled for loops, if present, become intervals for the solver to deal with.
        // Also, any loop that is marked not to be partitioned becomes an interval for the solver
        // (this happens to the inner loop of a split, such as split parallel execution).
        if (op->loop_split.may_be_split() && 
            (op->for_type == For::Serial || op->for_type == For::Parallel) ){
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
            expr = new Min(new Solve(a, DomInterval(make_infinity(b.type(), -1), b, true)), b);
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
            expr = new Max(new Solve(a, DomInterval(b, make_infinity(b.type(), +1), true)), b);
        } else if (a.same_as(op->a) && b.same_as(op->b)) {
            expr = op;
        } else {
            expr = new Max(a, b);
        }
    }

# if LOOP_SPLIT_CONDITIONAL    
    // Solve an inequality.
    // All comparisons should first be simplified to LT or EQ.
    // LT can be eliminated in a loop body if it always evaluated either to true or to false.
    // This means that LT generates two solutions: one for < and one for >=.  Which solution is
    // relevant is difficult to determine.
    virtual void visit(const LT *op) {
        Expr a = mutate(op->a);
        Expr b = mutate(op->b);
        if (is_constant_expr(a)) {
            // ka < b: Solve for b on (ka+1,infinity) [integer types] or (ka,infinity) [float]
            DomInterval bounds_a = bounds.bounds(a);
            Expr limit = bounds_a.max; // Must exceed the maximum of the other side.
            if (b.type().is_int() || b.type().is_uint()) {
                limit = simplify(limit + make_one(b.type()));
            }
            expr = new LT(a, new Solve(b, DomInterval(limit, make_infinity(b.type(), +1), true)));
        } else if (is_constant_expr(b)) {
            // a < kb: Solve for a on (-infinity, kb-1) or (-infinity,kb)
            DomInterval bounds_b = bounds.bounds(b);
            Expr limit = bounds_b.min; // Must stay below the minimum of the other side.
            if (a.type().is_int() || a.type().is_uint()) {
                limit = simplify(limit - make_one(a.type()));
            }
            expr = new LT(new Solve(a, DomInterval(make_infinity(a.type(), -1), limit, true)), b);
        } else if (a.same_as(op->a) && b.same_as(op->b)) {
            expr = op;
        } else {
            expr = new LT(a, b);
        }
    }
# endif
    
    // Do not solve for equality/inequality because it does not generate a useful
    // loop partition (a single case is isolated).  Note that, for small loops,
    // loop unrolling may make each case individual; then each case is optimised separately
    // anhow and can be optimised.  So... if a code module uses equality tests on
    // an index variable then a likely candidate for optimisation is to unroll the variable
    // first.
    
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
            expr = new Mod(new Solve(a, DomInterval(make_zero(a.type()), limit, true)), b);
        } else if (is_negative_const(b)) {
            // a % kb: Solve for a on (kb+1,0) or (kb,0)
            Expr limit = b;
            if (a.type().is_int() || a.type().is_uint()) {
                limit = simplify(limit + make_one(a.type()));
            }
            expr = new Mod(new Solve(a, DomInterval(limit, make_zero(a.type()), true)), b);
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
            expr = new Clamp(op->clamptype, new Solve(a, DomInterval(min, max, true)), min, max, p1);
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
class LoopSplitting : public IRMutator {
    using IRMutator::visit;
private:
# define MAIN_NEST_INIT 0
# define MAIN_NEST_IN_MAIN 1
# define MAIN_NEST_IN_OTHER 2
    int main_nest_state; // When option loop_main_separate is true, use this variable to manage state of code generation
# define STATUS_NO_LOOPS 0
# define STATUS_SPLIT_LOOPS 1
    int return_status; // Return a status from nested mutation to see whether there are loops in there or not.
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
        if (infinity_count(point) != 0)
            return; // No interest in collecting infinite points
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
    void insert_partition_point(Expr point, bool hint_end, int ithresh, int &num_start, int &num_end, std::vector<Expr> &starts, std::vector<Expr> &ends) {
        int ival;
        if (get_const_int(point, ival)) {
            // A numeric value.  
            if (ival < ithresh && ival > num_start) num_start = ival;
            else if (ival >= ithresh && ival < num_end) num_end = ival;
#if 0
        } else if (has_variable_match(".min.", point)) {
            // Symbolic expression.
            if (has_variable_match(".extent.", point)) {
                // End point.
                insert_partition_point(point, ends, true);
            } else {
                // Start point.
                insert_partition_point(point, starts, false);
            }
#endif
        } else {
            if (hint_end) {
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
                insert_partition_point(start, false, ithresh, num_start, num_end, starts, ends);
                insert_partition_point(end, true, ithresh, num_start, num_end, starts, ends);
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
        //Stmt new_body = mutate(op->body);
        if ((op->for_type == For::Serial || 
            (global_options.loop_split_parallel && op->for_type == For::Parallel)) &&
            (op->loop_split.status == LoopSplitInfo::Ordinary)) {
            log(LOGLEVEL) << "Considering splitting loop:" << op << "\n" << Stmt(op);
            DomInterval part;
            if (op->loop_split.interval_defined()) {
                part = op->loop_split.interval;
                log(LOGLEVEL) << "Manual split " << part << "\n";
            } else if (op->loop_split.auto_split == LoopSplitInfo::Yes || 
                      (op->loop_split.auto_split == LoopSplitInfo::Undefined && global_options.loop_split_all)) {
                // Automatic loop splitting.  Determine an interval for the main loop.

                // Search for solutions related to this particular for loop.
                //std::vector<Solution> solutions = extract_solutions(op->name, Stmt(), solved);
                std::vector<Solution> solutions = extract_solutions(op->name, op, solved);
                //std::cout << global_options;
                log(LOGLEVEL) << "Considering automatic loop splitting\n";
# if 1
                log(LOGLEVEL) << "Solutions: \n";
                for (size_t i = 0; i < solutions.size(); i++) {
                    log(LOGLEVEL) << solutions[i].var << " " << solutions[i].intervals << "\n";
                }
# endif               
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
# if 0
                std::cout << "Main loop start: ";
                for (size_t i = 0; i < starts.size(); i++) {
                    std::cout << starts[i] << " ";
                }
                std::cout << "\n" << "Main loop end: ";
                for (size_t i = 0; i < ends.size(); i++) {
                    std::cout << ends[i] << " ";
                }
                std::cout << "\n";
# endif               
                // The interval for the main loop is max(starts) to min(ends)-1.
                // If no partition points are found then the interval has undefined min and/or max.
                Expr part_start = make_infinity(Int(32), -1), part_end = make_infinity(Int(32), 1);
                if (starts.size() > 0) {
                    part_start = starts[0];
                    for (size_t i = 1; i < starts.size(); i++) {
                        part_start = max (part_start, starts[i]);
                    }
                }
                if (ends.size() > 0) {
                    part_end = ends[0];
                    for (size_t i = 1; i < ends.size(); i++) {
                        part_end = min (part_end, ends[i]);
                    }
                    part_end = part_end - 1;
                }
                
                // Exactness of partition poiunts is not important
                part = DomInterval(simplify(part_start), simplify(part_end), true);
                
                log(LOGLEVEL) << "Auto partition: " << op->name << " " << part << "\n";
            }
            
            if ((part.min.defined() && infinity_count(part.min) == 0) || 
                (part.max.defined() && infinity_count(part.max) == 0)) { 
                
                // Here is the point where we have decided to split this loop.
                // If the global option loop_main_separate is true and if the
                // main_nest_state is MAIN_NEST_INIT then we need to initialise the
                // separate loops.
                if (global_options.loop_main_separate && main_nest_state == MAIN_NEST_INIT) {
                    log(LOGLEVEL) << "Initialise separate main loop:\n" << Stmt(op);
                    main_nest_state = MAIN_NEST_IN_MAIN;
                    Stmt main = mutate(Stmt(op));
                    main_nest_state = MAIN_NEST_IN_OTHER;
                    Stmt other = mutate(Stmt(op));
                    main_nest_state = MAIN_NEST_INIT;
                    // Construct the code block to replace the loop.
                    Stmt block;
                    append_stmt(block, other); // Could have a problem here with empty code block generated??
                    append_stmt(block, main);
                    stmt = block;
                    done = true;
                } else {
                    // Nested computation of return_status
                    int save_return_status = return_status;
                    return_status = STATUS_NO_LOOPS; // Initialise for nested call.
                    Stmt new_body = mutate(op->body);
                    int new_return_status = return_status;
                    return_status = save_return_status;
                log(LOGLEVEL) << "About to partition loop:\n" << Stmt(op);
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
                if (global_options.loop_split_letbind) {
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
                LoopSplitInfo p = op->loop_split;
                // Generate the start loop but not if loop_main_separate is true and the nest state is IN_MAIN
                if (start.defined() && ! (global_options.loop_main_separate && main_nest_state == MAIN_NEST_IN_MAIN)) {
                    p.status = LoopSplitInfo::Before;
                    append_stmt(block, new For(op->name, before_min, before_extent, op->for_type, p, op->body));
                    // Test: Split before and after of parallel loop to serial.
                    // Limited testing suggests this is faster than splitting to a full parallel
                    // loop (for cases where before and after are small parallel loops, probably 2
                    // iterations) but it is even faster not to split parallel loops at all.
                    //append_stmt(block, new For(op->name, before_min, before_extent, For::Serial, p, op->body));
                    return_status = STATUS_SPLIT_LOOPS;
                }
                p.status = LoopSplitInfo::Main;
                // Generate the main loop but not if loop_main_separate is true and nest state is IN_OTHER
                // unless new_return_status is STATUS_SPLIT_LOOPS (meaning that new_body contains split loops)
                if (! (global_options.loop_main_separate && main_nest_state == MAIN_NEST_IN_OTHER) ||
                    new_return_status == STATUS_SPLIT_LOOPS) {
                    append_stmt(block, new For(op->name, main_min, main_extent, op->for_type, p, new_body));
                    // If we are including the main loop because it contains other side loops, then report that in the status 
                    if (main_nest_state == MAIN_NEST_IN_OTHER)
                        return_status = STATUS_SPLIT_LOOPS;
                }
                p.status = LoopSplitInfo::After;
                // Generate the end loop but not if loop_main_separate is true and the nest state is IN_MAIN
                if (end.defined() && ! (global_options.loop_main_separate && main_nest_state == MAIN_NEST_IN_MAIN)) {
                    append_stmt(block, new For(op->name, after_min, after_extent, op->for_type, p, op->body));
                    //append_stmt(block, new For(op->name, after_min, after_extent, For::Serial, p, op->body));
                    return_status = STATUS_SPLIT_LOOPS;
                }
                // Let bindings that appear before the loops...
                if (global_options.loop_split_letbind) {
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
        }
        if (! done) {
            Stmt new_body = mutate(op->body);
            if (new_body.same_as(op->body)) {
                stmt = op;
            } else {
                stmt = new For(op, op->min, op->extent, new_body);
            }
        }
    }
public:
    LoopSplitting() { main_nest_state = MAIN_NEST_INIT; }

};

Stmt loop_split(Stmt s) {
    //return LoopSplitting().mutate(s);
    s = simplify(s); // Must be fully simplified first
    code_logger.log(s, "simplify");
    code_logger.section("pre_solver");
    Stmt pre = LoopPreSolver().mutate(s);
    code_logger.log(pre, "pre_solver");
    code_logger.section("solved");
    Stmt solved = loop_solver(pre);
    code_logger.log(solved, "solved");
    LoopSplitting loop_part;
    loop_part.solved = solved;
    code_logger.section("loop_partition");
    s = loop_part.mutate(s);
    code_logger.log(s, "loop_partition");
    return s;
}

// Test loop partition routines.

// Test pre processing.
namespace {
Stmt code_1 () {
    Type i32 = Int(32);
    Expr x = new Variable(Int(32), "x");
    Expr y = new Variable(Int(32), "y");
    Expr input = new Call(Int(16), "input", vec((x - 10) % 100 + 10));
    Expr select = new Select(x > 3, new Select(x < 87, input, new Cast(Int(16), -17)),
                             new Cast(Int(16), -17));
    Stmt store = new Store("buf", select, x - 1);
    LoopSplitInfo autosplit(true); // Select auto loop spliting.
    Stmt for_loop = new For("x", 0, 100, For::Serial, autosplit, store);
    Expr call = new Call(i32, "buf", vec(max(min(x,100),0)));
    Expr call2 = new Call(i32, "buf", vec(max(min(x-1,100),0)));
    Expr call3 = new Call(i32, "buf", vec(Expr(new Clamp(Clamp::Reflect, x+1, 0, 100))));
    Stmt store2 = new Store("out", call + call2 + call3 + 23, x);
    LoopSplitInfo manualsplit(DomInterval(1,99,true)); // Specify manual partitioning interval.
    Stmt for_loop2 = new For("x", 0, 100, For::Serial, manualsplit, store2);
    Stmt pipeline = new Pipeline("buf", for_loop, Stmt(), for_loop2);
    
    return pipeline;
}

Stmt code_2() {
    Expr x = new Variable(Int(32), "x");
    Expr y = new Variable(Int(32), "y");
    Expr input1 = new Call(Int(16), "input", vec((x - 10) % 100, Expr(new Clamp(Clamp::Replicate, y-3, 0, 100))));
    Expr input2 = new Call(Int(16), "input", vec((x + 5) % 100, Expr(new Clamp(Clamp::Replicate, y+2, 0, 100))));
    Stmt store = new Store("buf", input1 + input2, y * 100 + x);
    LoopSplitInfo autosplit(true); // Select auto loop spliting.
    Stmt inner_loop = new For("x", 0, 100, For::Serial, autosplit, store);
    Stmt outer_loop = new For("y", 0, 100, For::Serial, autosplit, inner_loop);
    return outer_loop;
}

// Selection of the appropriate solutions.
// LOOP_SPLIT_CONDITIONAL - defined above
// LIFT_CONSTANTS_MIN_MAX: If true, constants are lifted outside min and max.  Defined in Simplify.h
# if (LIFT_CONSTANT_MIN_MAX == 0 && LOOP_SPLIT_CONDITIONAL == 0)
std::string correct_simplified =
"produce buf {\n"
"  for (x, 0, 100, auto [-infinity, infinity]) {\n"
"    buf[(x + -1)] = select((3 < x), select((x < 87), input((((x + -10) % 100) + 10)), i16(-17)), i16(-17))\n"
"  }\n"
"} consume {\n"
"  for (x, 0, 100, [1, 99]) {\n"
"    out[x] = (((buf(max(min(x, 100), 0)) + buf(max(min((x + -1), 100), 0))) + buf(Clamp::reflect((x + 1),0,100,0))) + 23)\n"
"  }\n"
"}\n";

std::string correct_presolver =
"produce buf {\n"
"  for (x, 0, 100, auto [-infinity, infinity]) {\n"
"    stmtTargetVar(x) {\n"
"      buf[(x + -1)] = select((3 < x), select((x < 87), input(((solve([0, 99]: (x + -10)) % 100) + 10)), i16(-17)), i16(-17))\n"
"    }\n"
"  }\n"
"} consume {\n"
"  for (x, 0, 100, [1, 99]) {\n"
"    stmtTargetVar(x) {\n"
"      out[x] = (((buf(max(solve([0, infinity]: min(solve([-infinity, 100]: x), 100)), 0)) + buf(max(solve([0, infinity]: min(solve([-infinity, 100]: (x + -1)), 100)), 0))) + buf(Clamp::reflect(solve([0, 100]: (x + 1)),0,100,0))) + 23)\n"
"    }\n"
"  }\n"
"}\n";

std::string correct_solved =
"produce buf {\n"
"  for (x, 0, 100, auto [-infinity, infinity]) {\n"
"    stmtTargetVar(x) {\n"
"      buf[(x + -1)] = select((3 < x), select((x < 87), input((((solve([10, 109]: x) + -10) % 100) + 10)), i16(-17)), i16(-17))\n"
"    }\n"
"  }\n"
"} consume {\n"
"  for (x, 0, 100, [1, 99]) {\n"
"    stmtTargetVar(x) {\n"
"      out[x] = (((buf(max(min(solve([0, infinity]: solve([-infinity, 100]: x)), 100), 0)) + buf(max(min((solve([1, infinity]: solve([-infinity, 101]: x)) + -1), 100), 0))) + buf(Clamp::reflect((solve([-1, 99]: x) + 1),0,100,0))) + 23)\n"
"    }\n"
"  }\n"
"}\n";

// NOTE: The partitioned loop is not yet efficient.  Efficiency is introduced by
// BoundsSimplify which uses bounds analysis to simplify the min and max expressions.
std::string correct_loop_split = 
"produce buf {\n"
"  let x.start = 10\n"
"  let x.end = 110\n"
"  for (x, 0, min((x.start - 0), 100), before) {\n"
"    buf[(x + -1)] = select((3 < x), select((x < 87), input((((x + -10) % 100) + 10)), i16(-17)), i16(-17))\n"
"  }\n"
"  for (x, max(x.start, 0), (min(x.end, (0 + 100)) - max(x.start, 0)), main auto [-infinity, infinity]) {\n"
"    buf[(x + -1)] = select((3 < x), select((x < 87), input((((x + -10) % 100) + 10)), i16(-17)), i16(-17))\n"
"  }\n"
"  for (x, x.end, ((0 + 100) - x.end), after) {\n"
"    buf[(x + -1)] = select((3 < x), select((x < 87), input((((x + -10) % 100) + 10)), i16(-17)), i16(-17))\n"
"  }\n"
"} consume {\n"
"  let x.start = 1\n"
"  let x.end = 100\n"
"  for (x, 0, min((x.start - 0), 100), before) {\n"
"    out[x] = (((buf(max(min(x, 100), 0)) + buf(max(min((x + -1), 100), 0))) + buf(Clamp::reflect((x + 1),0,100,0))) + 23)\n"
"  }\n"
"  for (x, max(x.start, 0), (min(x.end, (0 + 100)) - max(x.start, 0)), main [1, 99]) {\n"
"    out[x] = (((buf(max(min(x, 100), 0)) + buf(max(min((x + -1), 100), 0))) + buf(Clamp::reflect((x + 1),0,100,0))) + 23)\n"
"  }\n"
"  for (x, x.end, ((0 + 100) - x.end), after) {\n"
"    out[x] = (((buf(max(min(x, 100), 0)) + buf(max(min((x + -1), 100), 0))) + buf(Clamp::reflect((x + 1),0,100,0))) + 23)\n"
"  }\n"
"}\n";
# endif

# if (LIFT_CONSTANT_MIN_MAX == 0 && LOOP_SPLIT_CONDITIONAL == 1)
std::string correct_simplified =
"produce buf {\n"
"  for (x, 0, 100, auto [-infinity, infinity]) {\n"
"    buf[(x + -1)] = select((3 < x), select((x < 87), input((((x + -10) % 100) + 10)), i16(-17)), i16(-17))\n"
"  }\n"
"} consume {\n"
"  for (x, 0, 100, [1, 99]) {\n"
"    out[x] = (((buf(max(min(x, 100), 0)) + buf(max(min((x + -1), 100), 0))) + buf(Clamp::reflect((x + 1),0,100,0))) + 23)\n"
"  }\n"
"}\n";

std::string correct_presolver =
"produce buf {\n"
"  for (x, 0, 100, auto [-infinity, infinity]) {\n"
"    stmtTargetVar(x) {\n"
"      buf[(x + -1)] = select((3 < solve([4, infinity]: x)), select((solve([-infinity, 86]: x) < 87), input(((solve([0, 99]: (x + -10)) % 100) + 10)), i16(-17)), i16(-17))\n"
"    }\n"
"  }\n"
"} consume {\n"
"  for (x, 0, 100, [1, 99]) {\n"
"    stmtTargetVar(x) {\n"
"      out[x] = (((buf(max(solve([0, infinity]: min(solve([-infinity, 100]: x), 100)), 0)) + buf(max(solve([0, infinity]: min(solve([-infinity, 100]: (x + -1)), 100)), 0))) + buf(Clamp::reflect(solve([0, 100]: (x + 1)),0,100,0))) + 23)\n"
"    }\n"
"  }\n"
"}\n";

std::string correct_solved =
"produce buf {\n"
"  for (x, 0, 100, auto [-infinity, infinity]) {\n"
"    stmtTargetVar(x) {\n"
"      buf[(x + -1)] = select((3 < solve([4, infinity]: x)), select((solve([-infinity, 86]: x) < 87), input((((solve([10, 109]: x) + -10) % 100) + 10)), i16(-17)), i16(-17))\n"
"    }\n"
"  }\n"
"} consume {\n"
"  for (x, 0, 100, [1, 99]) {\n"
"    stmtTargetVar(x) {\n"
"      out[x] = (((buf(max(min(solve([0, infinity]: solve([-infinity, 100]: x)), 100), 0)) + buf(max(min((solve([1, infinity]: solve([-infinity, 101]: x)) + -1), 100), 0))) + buf(Clamp::reflect((solve([-1, 99]: x) + 1),0,100,0))) + 23)\n"
"    }\n"
"  }\n"
"}\n";

// NOTE: The partitioned loop is not yet efficient.  Efficiency is introduced by
// BoundsSimplify which uses bounds analysis to simplify the min and max expressions.
std::string correct_loop_split = 
"produce buf {\n"
"  let x.start = 10\n"
"  let x.end = 87\n"
"  for (x, 0, min((x.start - 0), 100), before) {\n"
"    buf[(x + -1)] = select((3 < x), select((x < 87), input((((x + -10) % 100) + 10)), i16(-17)), i16(-17))\n"
"  }\n"
"  for (x, max(x.start, 0), (min(x.end, (0 + 100)) - max(x.start, 0)), main auto [-infinity, infinity]) {\n"
"    buf[(x + -1)] = select((3 < x), select((x < 87), input((((x + -10) % 100) + 10)), i16(-17)), i16(-17))\n"
"  }\n"
"  for (x, x.end, ((0 + 100) - x.end), after) {\n"
"    buf[(x + -1)] = select((3 < x), select((x < 87), input((((x + -10) % 100) + 10)), i16(-17)), i16(-17))\n"
"  }\n"
"} consume {\n"
"  let x.start = 1\n"
"  let x.end = 100\n"
"  for (x, 0, min((x.start - 0), 100), before) {\n"
"    out[x] = (((buf(max(min(x, 100), 0)) + buf(max(min((x + -1), 100), 0))) + buf(Clamp::reflect((x + 1),0,100,0))) + 23)\n"
"  }\n"
"  for (x, max(x.start, 0), (min(x.end, (0 + 100)) - max(x.start, 0)), main [1, 99]) {\n"
"    out[x] = (((buf(max(min(x, 100), 0)) + buf(max(min((x + -1), 100), 0))) + buf(Clamp::reflect((x + 1),0,100,0))) + 23)\n"
"  }\n"
"  for (x, x.end, ((0 + 100) - x.end), after) {\n"
"    out[x] = (((buf(max(min(x, 100), 0)) + buf(max(min((x + -1), 100), 0))) + buf(Clamp::reflect((x + 1),0,100,0))) + 23)\n"
"  }\n"
"}\n";
# endif

# if (LIFT_CONSTANT_MIN_MAX == 1 && LOOP_SPLIT_CONDITIONAL == 1)
// Alternate solution where additional simplification rules are active, lifting constant addends outside min and max.
std::string correct_simplified = 
"produce buf {\n"
"  for (x, 0, 100, auto [-infinity, infinity]) {\n"
"    buf[(x + -1)] = select((3 < x), select((x < 87), input((((x + -10) % 100) + 10)), i16(-17)), i16(-17))\n"
"  }\n"
"} consume {\n"
"  for (x, 0, 100, [1, 99]) {\n"
"    out[x] = (((buf(max(min(x, 100), 0)) + buf((max(min(x, 101), 1) + -1))) + buf(Clamp::reflect((x + 1),0,100,0))) + 23)\n"
"  }\n"
"}\n";

std::string correct_presolver =
"produce buf {\n"
"  for (x, 0, 100, auto [-infinity, infinity]) {\n"
"    stmtTargetVar(x) {\n"
"      buf[(x + -1)] = select((3 < solve([4, infinity]: x)), select((solve([-infinity, 86]: x) < 87), input(((solve([0, 99]: (x + -10)) % 100) + 10)), i16(-17)), i16(-17))\n"
"    }\n"
"  }\n"
"} consume {\n"
"  for (x, 0, 100, [1, 99]) {\n"
"    stmtTargetVar(x) {\n"
"      out[x] = (((buf(max(solve([0, infinity]: min(solve([-infinity, 100]: x), 100)), 0)) + buf((max(solve([1, infinity]: min(solve([-infinity, 101]: x), 101)), 1) + -1))) + buf(Clamp::reflect(solve([0, 100]: (x + 1)),0,100,0))) + 23)\n"
"    }\n"
"  }\n"
"}\n";

std::string correct_solved =
"produce buf {\n"
"  for (x, 0, 100, auto [-infinity, infinity]) {\n"
"    stmtTargetVar(x) {\n"
"      buf[(x + -1)] = select((3 < solve([4, infinity]: x)), select((solve([-infinity, 86]: x) < 87), input((((solve([10, 109]: x) + -10) % 100) + 10)), i16(-17)), i16(-17))\n"
"    }\n"
"  }\n"
"} consume {\n"
"  for (x, 0, 100, [1, 99]) {\n"
"    stmtTargetVar(x) {\n"
"      out[x] = (((buf(max(min(solve([0, infinity]: solve([-infinity, 100]: x)), 100), 0)) + buf((max(min(solve([1, infinity]: solve([-infinity, 101]: x)), 101), 1) + -1))) + buf(Clamp::reflect((solve([-1, 99]: x) + 1),0,100,0))) + 23)\n"
"    }\n"
"  }\n"
"}\n";

std::string correct_loop_split = 
"produce buf {\n"
"  let x.start = 10\n"
"  let x.end = 87\n"
"  for (x, 0, min((x.start - 0), 100), before) {\n"
"    buf[(x + -1)] = select((3 < x), select((x < 87), input((((x + -10) % 100) + 10)), i16(-17)), i16(-17))\n"
"  }\n"
"  for (x, max(x.start, 0), (min(x.end, (0 + 100)) - max(x.start, 0)), main auto [-infinity, infinity]) {\n"
"    buf[(x + -1)] = select((3 < x), select((x < 87), input((((x + -10) % 100) + 10)), i16(-17)), i16(-17))\n"
"  }\n"
"  for (x, x.end, ((0 + 100) - x.end), after) {\n"
"    buf[(x + -1)] = select((3 < x), select((x < 87), input((((x + -10) % 100) + 10)), i16(-17)), i16(-17))\n"
"  }\n"
"} consume {\n"
"  let x.start = 1\n"
"  let x.end = 100\n"
"  for (x, 0, min((x.start - 0), 100), before) {\n"
"    out[x] = (((buf(max(min(x, 100), 0)) + buf((max(min(x, 101), 1) + -1))) + buf(Clamp::reflect((x + 1),0,100,0))) + 23)\n"
"  }\n"
"  for (x, max(x.start, 0), (min(x.end, (0 + 100)) - max(x.start, 0)), main [1, 99]) {\n"
"    out[x] = (((buf(max(min(x, 100), 0)) + buf((max(min(x, 101), 1) + -1))) + buf(Clamp::reflect((x + 1),0,100,0))) + 23)\n"
"  }\n"
"  for (x, x.end, ((0 + 100) - x.end), after) {\n"
"    out[x] = (((buf(max(min(x, 100), 0)) + buf((max(min(x, 101), 1) + -1))) + buf(Clamp::reflect((x + 1),0,100,0))) + 23)\n"
"  }\n"
"}\n";
# endif


void code_compare (std::string long_desc, std::string head, Stmt code, std::string correct) {
    std::ostringstream output;
    output << code;
    std::string check = output.str();
    if (check != correct) {
        std::cerr << "Incorrect results from " << long_desc << "\n";
        std::cerr << head << "\n" << check << "\n";
        std::cerr << "Expected:\n" << correct << "\n";
        int ch = 0, line = 1;
        for (size_t i = 0; i < correct.size() && i < check.size(); i++) {
            if (check[i] != correct[i]) {
                std::cerr << "Difference at byte " << i << "  line " << line << "  pos " << ch << "\n";
                std::cerr << head << " " << check.substr(i, 30) << "...\n";
                std::cerr << "Expected:    " << correct.substr(i, 30) << "...\n";
                break;
            }
            if (check[i] == '\n') { line++; ch = 0; }
            else { ch++; }
        }
        if (correct.size() != check.size()) {
            std::cerr << "Different lengths " << correct.size() << " " << check.size() << "\n";
        }

        // Hack: until we get the correct results of the test
        assert(0 && "Code incorrect");
    }
}

}

#include <sstream>

void test_loop_split_1() {
    // Remember the global options and override them for this test.
    Options the_options = global_options;
    global_options = Options();
    global_options.lift_let = false;
    global_options.loop_split_letbind = false;
    global_options.loop_split = true;
    global_options.loop_split_all = false;
    
    Stmt pipeline = code_1();
    
    //std::cout << "Raw:\n" << pipeline << "\n";
    Stmt simp = simplify(pipeline);
    code_compare ("simplify called from loop split", "Simplified code:", simp, correct_simplified);
    //std::cout << "Simplified:\n" << simp << "\n";
    Stmt pre = LoopPreSolver().mutate(simp);
    //std::cout << "LoopPreSolver:\n" << pre << "\n";
    code_compare ("loop split pre-solver", "Presolved code:", pre, correct_presolver);
    
    Stmt solved = loop_solver(pre);
    code_compare ("loop split solver", "Solved code:", solved, correct_solved);
    //std::cout << "Solved:\n" << solved << "\n";
    //std::vector<Solution> solutions = extract_solutions("x", Stmt(), solved);
    //for (size_t i = 0; i < solutions.size(); i++) {
    //    std::cout << solutions[i].var << " " << solutions[i].intervals << "\n";
    //}
    
    // Note: Loop splitting does not actually improve the code inside the loops.
    // That is done by interval analysis simplification on the code after loop splitting.
    LoopSplitting loop_part;
    loop_part.solved = solved;
    Stmt part = loop_part.mutate(simp);
    code_compare ("loop splitting", "Loop Split:", part, correct_loop_split);
    
    // ------------- SECOND TEST ----------------
    global_options = Options();
    global_options.lift_let = false;
    global_options.loop_split_letbind = false;
    global_options.loop_split = true;
    global_options.loop_split_all = false;
    global_options.loop_main_separate = true;
    
    Stmt code2 = code_2();

    std::cout << "Raw:\n" << code2 << "\n";
    Stmt simp2 = simplify(code2);
    //code_compare ("simplify called from loop split", "Simplified code:", simp2, correct_simplified2);
    std::cout << "Simplified:\n" << simp2 << "\n";
    Stmt pre2 = LoopPreSolver().mutate(simp2);
    std::cout << "LoopPreSolver:\n" << pre2 << "\n";
    //code_compare ("loop split pre-solver", "Presolved code:", pre2, correct_presolver2);
    
    Stmt solved2 = loop_solver(pre2);
    //code_compare ("loop split solver", "Solved code:", solved2, correct_solved2);
    std::cout << "Solved:\n" << solved2 << "\n";
    //std::vector<Solution> solutions22 = extract_solutions("x", Stmt(), solved2);
    //for (size_t i = 0; i < solutions.size(); i++) {
    //    std::cout << solutions2[i].var << " " << solutions2[i].intervals << "\n";
    //}
    
    // Note: Loop splitting does not actually improve the code inside the loops.
    // That is done by interval analysis simplification on the code after loop splitting.
    LoopSplitting loop_part2;
    loop_part.solved = solved2;
    Stmt part2 = loop_part.mutate(simp2);
    std::cout << "Loop Split:\n" << part2 << "\n";
    //code_compare ("loop splitting", "Loop Split:", part2, correct_loop_split)2;
    
    global_options = the_options;
}

void loop_split_test() {
    test_loop_split_1();
    
    std::cout << "Loop Partition test passed\n";
}

class EffectivePartition : public IRVisitor {
public:
    int failing;
    
    EffectivePartition() : failing(false) {}
private:
    using IRVisitor::visit;
    
    virtual void visit(const For *op) {
        // For loops that are marked Main are worthy of consideration.
        // For loops marked Before or After are not.
        // For loops that are marked not to be partitioned are worthy of consideration -
        // these arise from variable splitting.
        // Potential problems: The Before or After loops may be degenerate.
        if (op->loop_split.status == LoopSplitInfo::Main || ! op->loop_split.may_be_split()) {
            op->body.accept(this);
        }
    }
    
    virtual void visit(const LetStmt *op) {
        // LetStmt is not part of the loop body (not usually)
        op->body.accept(this);
    }
    
    virtual void visit(const Min *op) {
        // Constant expression such as Min(ramp(), broadcast()) is acceptable.
        if (is_const(op->a) && is_const(op->b)) return;
        std::cerr << Expr(op) << "\n";
        failing = true;
    }
    
    virtual void visit(const Max *op) {
        // Constant expression such as Max(ramp(), broadcast()) is acceptable.
        if (is_const(op->a) && is_const(op->b)) return;
        std::cerr << Expr(op) << "\n";
        failing = true;
    }
    
    virtual void visit(const Clamp *op) {
        std::cerr << Expr(op) << "\n";
        failing = true;
    }
    
    virtual void visit(const Mod *op) {
        // Constant expression such as Mod(ramp(), broadcast()) is acceptable.
        if (is_const(op->a) && is_const(op->b)) return;
        std::cerr << Expr(op) << "\n";
        failing = true;
    }
    
    virtual void visit(const Select *op) {
        std::cerr << Expr(op) << "\n";
        failing = true;
    }
};


/** is_effective_split tells whether loop splitting has been effective to eliminate
 * Min, Max, Clamp, Select and Mod operators from the main loop.  Note that there are obviously
 * programs that use clamp(), Mod, or Select on data.  Such programs will not pass
 * this test, but the purpose of the test is for those programs that should be fully
 * clean after splitting has been applied. */
bool is_effective_loop_split(Stmt s) {
    EffectivePartition eff;
    s.accept(&eff);
    return !eff.failing;
}

}
}
