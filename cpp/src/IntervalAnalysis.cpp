#include "IntervalAnalysis.h"
#include "IRRewriter.h"
#include "IRMutator.h"
#include "IRVisitor.h"
#include "IR.h"
#include "IROperator.h"
#include "IREquality.h"
#include "Simplify.h"
#include "IRPrinter.h"
#include "Util.h"
#include "Var.h"
#include "Log.h"
#include <iostream>


// Interval analysis based on Bounds.cpp but modified to
// provide the information to another visitor that uses it.
// The subvisitor gets to see the tree nodes that it is interested in
// in a stratego-strategy kind of manner.
// Also modified to compute more interval analysis information than Bounds.cpp.

namespace Halide { 
namespace Internal {

using std::make_pair;
using std::map;
using std::vector;
using std::string;

class IntervalAnalysis : public IRMutator {
public:
    Expr min, max;
    Scope<Interval> scope;
    
    IRRewriter *rewriter;
    // Simplification removes unnecessary Min, Max, Select, ... nodes
    // Apply simplification after loop bounds have been determined.
    bool do_simplify;
    
    IntervalAnalysis() { rewriter = 0; do_simplify = false;}

    using IRVisitor::visit;

    void bounds_of_type(Type t) {
        if (t.is_uint()) {
            if (t.bits <= 16) {
                max = cast(t, (1 << t.bits) - 1);
                min = cast(t, 0);
            } else {
                max = Expr();
                min = Expr();
            }
        } else if (t.is_int()) {
            if (t.bits <= 16) {
                max = cast(t, (1 << (t.bits-1)) - 1);
                min = cast(t, -(1 << (t.bits-1)));
            }
        } else {
            max = Expr();
            min = Expr();
        }        
    }

    void visit(const IntImm *op) {
        // First thing: Bring the information up to date.
        min = op;
        max = op;
        // Then apply the rewriter.
        rewriter->visit(op);
        expr = rewriter->expr;
    }
    
    void visit(const FloatImm *op) {
        min = op;
        max = op;
        rewriter->visit(op);
        expr = rewriter->expr;
    }

    void visit(const Cast *op) {
        // Assume no overflow
        Expr value = mutate(op->value);
        min = min.defined() ? new Cast(op->type, min) : Expr();
        max = max.defined() ? new Cast(op->type, max) : Expr();
        
        if (value.same_as(op->value)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new Cast(op->type, value));
        }
        expr = rewriter->expr;
    }

    void visit(const Variable *op) {
        //log(0) << "Variable " << op->name << "\n";
        if (scope.contains(op->name)) {
            //log(0) << "Found in scope\n";
            Interval bounds = scope.get(op->name);
            min = bounds.min;
            max = bounds.max;
        } else {
            //log(0) << "Not found in scope\n";
            min = op;
            max = op;
        }
        rewriter->visit(op);
        expr = rewriter->expr;
    }

    void visit(const Add *op) {
        Expr a = mutate(op->a);
        Expr min_a = min, max_a = max;
        Expr b = mutate(op->b);

        min = (min.defined() && min_a.defined()) ? new Add(min_a, min) : Expr();
        max = (max.defined() && max_a.defined()) ? new Add(max_a, max) : Expr();
        
        if (a.same_as(op->a) && b.same_as(op->b)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new Add(a, b));
        }
        expr = rewriter->expr;
    }

    void visit(const Sub *op) {
        Expr a = mutate(op->a);
        Expr min_a = min, max_a = max;
        Expr b = mutate(op->b);
        Expr min_b = min, max_b = max;
        min = (max_b.defined() && min_a.defined()) ? new Sub(min_a, max_b) : Expr();
        max = (min_b.defined() && max_a.defined()) ? new Sub(max_a, min_b) : Expr();
        
        if (a.same_as(op->a) && b.same_as(op->b)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new Sub(a, b));
        }
        expr = rewriter->expr;
    }

    void visit(const Mul *op) {
        Expr a = mutate(op->a);
        Expr min_a = min, max_a = max;
        Expr b = mutate(op->b);
        Expr min_b = min, max_b = max;
        // Special-case optimizations to generate less work for the constant-folder
        if (is_const(a)) {
            if (is_negative_const(a)) std::swap(min_b, max_b);
            if (min_b.defined()) min = min_b * a;
            if (max_b.defined()) max = max_b * a;
        } else if (is_const(b)) {
            if (is_negative_const(b)) std::swap(min_a, max_a);
            if (min_a.defined()) min = min_a * b;
            if (max_a.defined()) max = max_a * b;            
        } else {
            // Loss of information: The following code assumes that an unknown at either
            // end of either range means an unknown result.  This is not quite true.  
            // e.g. If it is known that max_a is negative and min_b is positive then
            // we can determine the maximum of the result range (it will be negative).
            if (!min_a.defined() || !max_a.defined()) {
                min = Expr(); max = Expr();
            } else if (!min_b.defined() || !max_b.defined()) {
                min = Expr(); max = Expr();
            } else {
                
                Expr a = min_a * min_b;
                Expr b = min_a * max_b;
                Expr c = max_a * min_b;
                Expr d = max_a * max_b;
                
                min = new Min(new Min(a, b), new Min(c, d));
            max = new Max(new Max(a, b), new Max(c, d));
            }
        }
        
        if (a.same_as(op->a) && b.same_as(op->b)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new Mul(a, b));
        }
        expr = rewriter->expr;
    }

    void visit(const Div *op) {
        Expr a = mutate(op->a);
        Expr min_a = min, max_a = max;
        Expr b = mutate(op->b);
        Expr min_b = min, max_b = max;

        // As noted for Mul, the assumption that we cannot determine anything if one
        // of the limits is unknown is not necessarily true. 
        if (!min_a.defined() || !max_a.defined() || !min_b.defined() || !max_b.defined()) {
            min = Expr(); max = Expr();
        } else if (is_const(b)) {
            if (is_negative_const(b)) std::swap(min_a, max_a);                
            if (min_a.defined()) min = min_a / op->b;
            if (max_a.defined()) max = max_b / op->b;
        } else {
            // if we can't statically prove that the divisor can't span zero, then we're unbounded
            bool min_b_is_positive = proved(min_b > make_zero(min_b.type()));
            bool max_b_is_negative = proved(max_b < make_zero(max_b.type()));
            if (! min_b_is_positive && ! max_b_is_negative) {
                min = Expr();
                max = Expr();
            } else {
                Expr a = min_a / min_b;
                Expr b = min_a / max_b;
                Expr c = max_a / min_b;
                Expr d = max_a / max_b;
                
                min = new Min(new Min(a, b), new Min(c, d));
                max = new Max(new Max(a, b), new Max(c, d));
            }
        }

        if (a.same_as(op->a) && b.same_as(op->b)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new Div(a, b));
        }
        expr = rewriter->expr;
    }

    void visit(const Mod *op) {
        Expr a = mutate(op->a);
        Expr b = mutate(op->b);
        Expr min_b = min, max_b = max;

        if (! min_b.defined() || ! max_b.defined()) {
            min = max = Expr();
        } else {
            min = make_zero(op->type);
            if (!max_b.type().is_float()) {
                max = max_b - 1;
            }
        }

        if (a.same_as(op->a) && b.same_as(op->b)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new Mod(a, b));
        }
        expr = rewriter->expr;
    }

	//LH
	// Bounds of clamp expression
    void visit(const Clamp *op) {
        Expr a = mutate(op->a);
        Expr min_a = min, max_a = max;
        Expr lo = mutate(op->min);
        Expr min_lo = min, max_lo = max;
        Expr hi = mutate(op->max);
        Expr min_hi = min, max_hi = max;
        Expr p1 = op->p1;
        if (p1.defined()) { p1 = mutate(op->p1); }

        //log(3) << "Bounds of " << Expr(op) << "\n";

		// Bounds of result are no greater than bounds of hi
        if (min_a.defined() && min_hi.defined()) {
            min = new Min(min_a, min_hi);
        } else {
            min = Expr();
        }

        if (max_a.defined() && max_hi.defined()) {
            // If both hi and a have max defined, then the smaller of them applies.
            max = new Min(max_a, max_hi);
        } else {
            max = max_a.defined() ? max_a : max_hi;
        }

		// Bounds of result are no smaller than bounds of lo
        if (min.defined() && min_lo.defined()) {
            min = new Max(min, min_lo);
        } else {
            min = min.defined() ? min : min_lo;
        }

        if (max.defined() && max_lo.defined()) {
            max = new Max(max, max_lo);
        } else {
            max = Expr();
        }

        //log(3) << min << ", " << max << "\n";

        if (a.same_as(op->a) && lo.same_as(op->min) && hi.same_as(op->max) && p1.same_as(op->p1)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new Clamp(op->clamptype, a, lo, hi, p1));
        }
        expr = rewriter->expr;
    }


    void visit(const Min *op) {
        Expr a = mutate(op->a);
        Expr min_a = min, max_a = max;
        Expr b = mutate(op->b);
        Expr min_b = min, max_b = max;

        //log(3) << "Bounds of " << Expr(op) << "\n";

        if (min_a.defined() && min_b.defined()) {
            min = new Min(min_a, min_b);
        } else {
            min = Expr();
        }

        if (max_a.defined() && max_b.defined()) {
            max = new Min(max_a, max_b);
        } else {
            max = max_a.defined() ? max_a : max_b;
        }
        
        if (do_simplify && max_a.defined() && min_b.defined() && proved(max_a < min_b)) {
            // Intervals do not overlap. a is always the minimum
            expr = a;
            return;
        }
        if (do_simplify && max_b.defined() && min_a.defined() && proved(max_b < min_a)) {
            expr = b;
            return;
        }

        //log(3) << min << ", " << max << "\n";

        if (a.same_as(op->a) && b.same_as(op->b)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new Min(a, b));
        }
        expr = rewriter->expr;
    }


    void visit(const Max *op) {
        Expr a = mutate(op->a);
        Expr min_a = simplify_undef(min), max_a = simplify_undef(max);
        Expr b = mutate(op->b);
        Expr min_b = simplify_undef(min), max_b = simplify_undef(max);

        //log(3) << "Bounds of " << Expr(op) << "\n";

        if (min_a.defined() && min_b.defined()) {
            min = new Max(min_a, min_b);
        } else {
            min = min_a.defined() ? min_a : min_b;
        }

        if (max_a.defined() && max_b.defined()) {
            max = new Max(max_a, max_b);
        } else {
            max = Expr();
        }

        if (do_simplify && max_a.defined() && min_b.defined() && proved(max_a < min_b)) {
            // Intervals do not overlap. b is always the maximum
            expr = b;
            return;
        }
        if (do_simplify && max_b.defined() && min_a.defined() && proved(max_b < min_a)) {
            expr = a;
            return;
        }

        //log(3) << min << ", " << max << "\n";

        if (a.same_as(op->a) && b.same_as(op->b)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new Max(a, b));
        }
        expr = rewriter->expr;
    }

    void visit(const EQ *op) {
        Expr a = mutate(op->a);
        Expr min_a = simplify_undef(min), max_a = simplify_undef(max);
        Expr b = mutate(op->b);
        Expr min_b = simplify_undef(min), max_b = simplify_undef(max);
        if ((max_a.defined() && min_b.defined() && proved(max_a < min_b)) || 
            (max_b.defined() && min_a.defined() && proved(max_b < min_a))) {
            // Intervals do not overlap.
            min = max = const_false();
        } else if (equal(min_a, max_a) && equal(min_a, min_b) && equal(min_a, max_b)) {
            // Identical constants.
            min = max = const_true();
        } else {
            min = const_false();
            max = const_true();
        }

        if (a.same_as(op->a) && b.same_as(op->b)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new EQ(a, b));
        }
        expr = rewriter->expr;
    }

    void visit(const NE *op) {
        Expr a = mutate(op->a);
        Expr min_a = simplify_undef(min), max_a = simplify_undef(max);
        Expr b = mutate(op->b);
        Expr min_b = simplify_undef(min), max_b = simplify_undef(max);
        if ((max_a.defined() && min_b.defined() && proved(max_a < min_b)) || 
            (max_b.defined() && min_a.defined() && proved(max_b < min_a))) {
            // Intervals do not overlap.
            min = max = const_true();
        } else if (equal(min_a, max_a) && equal(min_a, min_b) && equal(min_a, max_b)) {
            // Identical constants.
            min = max = const_false();
        } else {
            min = const_false();
            max = const_true();
        }

        if (a.same_as(op->a) && b.same_as(op->b)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new NE(a, b));
        }
        expr = rewriter->expr;
    }

    void visit(const LT *op) {
        Expr a = mutate(op->a);
        Expr min_a = simplify_undef(min), max_a = simplify_undef(max);
        Expr b = mutate(op->b);
        Expr min_b = simplify_undef(min), max_b = simplify_undef(max);
        if (max_a.defined() && min_b.defined() && proved(max_a < min_b)) {
            // a always less than b
            min = max = const_true();
        } else if (max_b.defined() && min_a.defined() && proved(min_a >= max_b)) {
            // a never less than b
            min = max = const_false();
        } else {
            min = const_false();
            max = const_true();
        }

        if (a.same_as(op->a) && b.same_as(op->b)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new LT(a, b));
        }
        expr = rewriter->expr;
    }

    void visit(const LE *op) {
        Expr a = mutate(op->a);
        Expr min_a = simplify_undef(min), max_a = simplify_undef(max);
        Expr b = mutate(op->b);
        Expr min_b = simplify_undef(min), max_b = simplify_undef(max);
        if (max_a.defined() && min_b.defined() && proved(max_a <= min_b)) {
            // a always <= b
            min = max = const_true();
        } else if (max_b.defined() && min_a.defined() && proved(min_a > max_b)) {
            // a never <= b
            min = max = const_false();
        } else {
            min = const_false();
            max = const_true();
        }

        if (a.same_as(op->a) && b.same_as(op->b)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new LE(a, b));
        }
        expr = rewriter->expr;
    }

    void visit(const GT *op) {
        Expr a = mutate(op->a);
        Expr min_a = simplify_undef(min), max_a = simplify_undef(max);
        Expr b = mutate(op->b);
        Expr min_b = simplify_undef(min), max_b = simplify_undef(max);
        if (max_b.defined() && min_a.defined() && proved(min_a > max_b)) {
            // a always greater than b
            min = max = const_true();
        } else if (max_a.defined() && min_b.defined() && proved(max_a <= min_b)) {
            // a never greater than b
            min = max = const_false();
        } else {
            min = const_false();
            max = const_true();
        }

        if (a.same_as(op->a) && b.same_as(op->b)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new GT(a, b));
        }
        expr = rewriter->expr;
    }

    void visit(const GE *op) {
        Expr a = mutate(op->a);
        Expr min_a = simplify_undef(min), max_a = simplify_undef(max);
        Expr b = mutate(op->b);
        Expr min_b = simplify_undef(min), max_b = simplify_undef(max);
        if (max_b.defined() && min_a.defined() && proved(min_a >= max_b)) {
            // a always >= b
            min = max = const_true();
        } else if (max_a.defined() && min_b.defined() && proved(max_a < min_b)) {
            // a never >= b
            min = max = const_false();
        } else {
            min = const_false();
            max = const_true();
        }

        if (a.same_as(op->a) && b.same_as(op->b)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new GE(a, b));
        }
        expr = rewriter->expr;
    }

    void visit(const And *op) {
        Expr a = mutate(op->a);
        Expr min_a = simplify_undef(min), max_a = simplify_undef(max);
        Expr b = mutate(op->b);
        Expr min_b = simplify_undef(min), max_b = simplify_undef(max);
        if (is_zero(max_a) || is_zero(max_b)) {
            // Either provably false, result is provable false.
            min = max = const_false();
        } else if (is_one(min_a)) {
            // One provably true, result is the same as the other.
            min = min_b;
            max = max_b;
        } else {
            // Even if b is provably true, a is not so the range is false,true.
            min = const_false();
            max = const_true();
        }

        if (a.same_as(op->a) && b.same_as(op->b)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new And(a, b));
        }
        expr = rewriter->expr;
    }

    void visit(const Or *op) {
        Expr a = mutate(op->a);
        Expr min_a = simplify_undef(min), max_a = simplify_undef(max);
        Expr b = mutate(op->b);
        Expr min_b = simplify_undef(min), max_b = simplify_undef(max);
        if (is_one(min_a) || is_one(min_b)) {
            // Either provably true, result is provable true.
            min = max = const_true();
        } else if (is_zero(max_a)) {
            // One provably false, result is the same as the other.
            min = min_b;
            max = max_b;
        } else {
            // Even if b is provably false, a is not so the range is false,true.
            min = const_false();
            max = const_true();
        }

        if (a.same_as(op->a) && b.same_as(op->b)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new Or(a, b));
        }
        expr = rewriter->expr;
    }

    void visit(const Not *op) {
        Expr a = mutate(op->a);
        Expr min_a = simplify_undef(min), max_a = simplify_undef(max);
        if (equal(min_a, max_a)) {
            // Proveable as either true or false, result is negation
            min = max = new Not(max_a);
        } else {
            // Unknown, so result is unknown
            min = const_false();
            max = const_true();
        }

        if (a.same_as(op->a)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new Not(a));
        }
        expr = rewriter->expr;
    }

    void visit(const Select *op) {
        // Evaluate the condition first.
        Expr condition = mutate(op->condition);
        Expr min_cond = simplify_undef(min), max_cond = simplify_undef(max);
        
        Expr true_value = mutate(op->true_value);
        Expr min_a = min, max_a = max;
        
        Expr false_value = mutate(op->false_value);
        Expr min_b = min, max_b = max;
        
        log(3) << "select(" << op->condition << ",...) => select(" << condition << ",...) on (" 
            << min_cond << ", " << max_cond << ")\n";
        if (is_one(min_cond)) {
            // If the condition is provably true, then the range is copied from true_value.
            min = min_a;
            max = max_a;
            
            if (do_simplify) {
                log(4) << "Proved false: simplify\n";
                expr = true_value;
                return;
            } else log(4) << "Proved true but not simplified\n";
        } else if (is_zero(max_cond)) {
            // Condition is provably false, copy range from false_value.
            min = min_b;
            max = max_b;
            
            if (do_simplify) {
                log(4) << "Proved false: simplify\n";
                expr = false_value;
                return;
            } else log(4) << "Proved false but not simplified\n";
        } else {
            min = (min_b.defined() && min_a.defined()) ? new Min(min_b, min_a) : Expr();
            max = (max_b.defined() && max_a.defined()) ? new Max(max_b, max_a) : Expr();
        }

        if (condition.same_as(op->condition) && true_value.same_as(op->true_value) && 
            false_value.same_as(op->false_value)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new Select(condition, true_value, false_value));
        }
        expr = rewriter->expr;
    }

    void visit(const Load *op) {
        bounds_of_type(op->type);
        rewriter->visit(op);
        expr = rewriter->expr;
    }

    void visit(const Ramp *op) {
        // Not handled.
        min = max = Expr();
        expr = op;
    }

    void visit(const Broadcast *op) {
        // Evaluate the interval of the underlying constant
        Expr value = mutate(op->value);

        if (value.same_as(op->value)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new Broadcast(value, op->width));
        }
        expr = rewriter->expr;
    }

    void visit(const Call *op) {
        // Could surely do better than simply take the bounds of the type!
        bounds_of_type(op->type);
        rewriter->visit(op);
        expr = rewriter->expr;
    }

    void visit(const Let *op) {
        Expr value = mutate(op->value);
        scope.push(op->name, Interval(min, max));
        // min and max returned from body are the resulting interval
        Expr body = mutate(op->body);
        scope.pop(op->name);

        if (value.same_as(op->value) && body.same_as(op->body)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new Let(op->name, value, body));
        }
        expr = rewriter->expr;
    }

    void visit(const LetStmt *op) {
        Expr value = mutate(op->value);
        scope.push(op->name, Interval(min, max));
        Stmt body = mutate(op->body);
        scope.pop(op->name);
        
        min = max = Expr();

        if (value.same_as(op->value) && body.same_as(op->body)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new LetStmt(op->name, value, body));
        }
        stmt = rewriter->stmt;
    }

    void visit(const PrintStmt *op) {
        // Not handled.
        min = max = Expr();
        stmt = op;
    }

    void visit(const AssertStmt *op) {
        // Not handled.
        min = max = Expr();
        stmt = op;
    }

    void visit(const Pipeline *op) {
        Stmt produce = mutate(op->produce);
        Stmt update = mutate(op->update);
        Stmt consume = mutate(op->consume);
        
        min = max = Expr();

        if (produce.same_as(op->produce) && update.same_as(op->update) && consume.same_as(op->consume)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new Pipeline(op->name, produce, update, consume));
        }
        stmt = rewriter->stmt;
    }

    void visit(const For *op) {
        // Compute interval for minimum of loop
        Expr begin = mutate(op->min);
        Expr begin_min = min; 
        Expr begin_max = max;
        // Compute interval for extent of loop
        Expr extent = mutate(op->extent);
       
        Expr end_max;
        if (max.defined() && begin_max.defined()) {
            end_max = begin_max + max;
        }
        else {
            end_max = Expr();
        }
        scope.push(op->name, Interval(begin_min, simplify_undef(end_max)));
        Stmt body = mutate(op->body);
        scope.pop(op->name);
        
        // Range of statement is undefined
        min = max = Expr();

        if (begin.same_as(op->min) && extent.same_as(op->extent) && body.same_as(op->body)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new For(op->name, begin, extent, op->for_type, body));
        }
        stmt = rewriter->stmt;
    }

    void visit(const Store *op) {
        Expr a = mutate(op->value);
        Expr b = mutate(op->index);
        
        min = max = Expr();

        if (a.same_as(op->value) && b.same_as(op->index)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new Store(op->name, a, b));
        }
        stmt = rewriter->stmt;
    }

    void visit(const Provide *op) {
        Expr value = mutate(op->value);
        
        min = max = Expr();
        
        if (value.same_as(op->value)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new Provide(op->name, value, op->args));
        }
        stmt = rewriter->stmt;
    }

    void visit(const Allocate *op) {
        Stmt body = mutate(op->body);
        
        min = max = Expr();
        
        if (body.same_as(op->body)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new Allocate(op->name, op->type, op->size, body));
        }
        stmt = rewriter->stmt;
    }

    void visit(const Realize *op) {
        Stmt body = mutate(op->body);
        
        min = max = Expr();
        
        if (body.same_as(op->body)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new Realize(op->name, op->type, op->bounds, body));
        }
        stmt = rewriter->stmt;
    }

    void visit(const Block *op) {
        Stmt first = mutate(op->first);
        Stmt rest = mutate(op->rest);
        
        min = max = Expr();
        
        if (first.same_as(op->first) && rest.same_as(op->rest)) {
            rewriter->visit(op);
        } else {
            rewriter->visit(new Block(first, rest));
        }
        stmt = rewriter->stmt;
    }
};


Interval interval_of_expr_in_scope(Expr expr, const Scope<Interval> &scope) {
    IRRewriter nochange;
    IntervalAnalysis b;
    b.scope = scope;
    b.rewriter = &nochange;
    b.mutate(expr); // Perform the mutation.
    return Interval(b.min, b.max);
}

Stmt interval_analysis_simplify(Stmt s) {
    IRRewriter nochange;
    IntervalAnalysis b;
    b.rewriter = &nochange;
    b.do_simplify = true;
    log::debug_level = 4;
    Stmt r = b.mutate(s); // Perform the mutation.
    log::debug_level = 0;
    return r;
}

Expr interval_analysis_simplify(Expr e) {
    IRRewriter nochange;
    IntervalAnalysis b;
    b.rewriter = &nochange;
    b.do_simplify = true;
    return b.mutate(e); // Perform the mutation.
}


namespace{
void check(const Scope<Interval> &scope, Expr e, Expr correct_min, Expr correct_max) {
    Interval result = interval_of_expr_in_scope(e, scope);
    if (result.min.defined()) result.min = simplify(result.min);
    if (result.max.defined()) result.max = simplify(result.max);
    bool success = true;
    if (!equal(result.min, correct_min)) {
        std::cout << "Incorrect min: " << result.min << std::endl
                  << "Should have been: " << correct_min << std::endl;
        success = false;
    }
    if (!equal(result.max, correct_max)) {
        std::cout << "Incorrect max: " << result.max << std::endl
                  << "Should have been: " << correct_max << std::endl;
        success = false;
    }
    if (! success) {
        std::cout << "Expression: " << e << std::endl;
    }
    assert(success && "Bounds test failed");
}
}

void interval_analysis_test() {
    Scope<Interval> scope;
    Var x("x"), y("y");
    scope.push("x", Interval(Expr(0), Expr(10)));

    //check(scope, Expr(2), 2, 2);
    check(scope, x, 0, 10);
    check(scope, x+1, 1, 11);
    check(scope, (x+1)*2, 2, 22);
    check(scope, x*x, 0, 100);
    check(scope, 5-x, -5, 5);
    check(scope, x*(5-x), -50, 50); // We don't expect bounds analysis to understand correlated terms
    check(scope, new Select(x < 4, x, x+100), 0, 110);
    check(scope, x+y, y, y+10);
    check(scope, x*y, new Min(y*10, 0), new Max(y*10, 0));
    check(scope, x/(x+y), Expr(), Expr());
    check(scope, 11/(x+1), 1, 11);
    check(scope, new Load(Int(8), "buf", x, Buffer(), Parameter()), cast(Int(8), -128), cast(Int(8), 127));
    check(scope, y + (new Let("y", x+3, y - x + 10)), y + 3, y + 23); // Once again, we don't know that y is correlated with x
    check(scope, clamp(1/(x-2), x-10, x+10), -10, 20);
    
    // Additional capabilities
    check(scope, new Select(x < 11, x, x+100), 0, 10); // select can be proved true
    check(scope, new Select(x == -1, x, x+100), 100, 110); // select can be proved false

    vector<Expr> input_site_1 = vec(2*x);
    vector<Expr> input_site_2 = vec(2*x+1);
    vector<Expr> output_site = vec(x+1);

    Stmt loop = new For("x", 3, 10, For::Serial, 
                        new Provide("output", 
                                    new Add(
                                        new Call(Int(32), "input", input_site_1),
                                        new Call(Int(32), "input", input_site_2)),
                                    output_site));

    std::cout << "Interval analysis test passed" << std::endl;
}

}
}
