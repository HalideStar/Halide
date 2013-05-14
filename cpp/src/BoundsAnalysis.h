#ifndef HALIDE_BOUNDS_ANALYSIS_H
#define HALIDE_BOUNDS_ANALYSIS_H

#include "IR.h"
#include "InfInterval.h"
#include "IREquality.h"
#include "IRLazyScope.h"
#include "IRPrinter.h"
#include "IROperator.h"
#include "Log.h"
#include "Simplify.h"
#include <map>

//# define LOGLEVEL 0
# define LOGLEVEL 4

namespace Halide { 
namespace Internal {

using std::string;

/** Class that performs bounds analysis using IRLazyScope.
 * This class can be called in the middle of a mutation or
 * other tree walk and will compute the bounds analysis result
 * for the passed tree node in the current context.  Note that this 
 * may involve walking the tree, and IRLazyScope will be used to 
 * track variable bindings and contexts while the bounds analysis
 * is being performed.  This may result in information being cached,
 * but that it why we use IRLazyScope and the ContextManager.
 *
 * This class uses InfInterval to represent the interval, with infinity
 * for unbounded intervals.
 */

class BoundsAnalysis : public IRLazyScope<IRProcess> {
    InfInterval interval;
    
    // Ideally the interval cache would be static and shared
    // in the same way as ContextManager.  It would then
    // want to be reset whenever the context manager is reset.
    // To do this in a tidy manner is difficult.  The untidy way
    // is to include it in the context manager.
    std::map<NodeKey, InfInterval> interval_cache;
    
public:
    InfInterval bounds(Expr e) { 
        // Insert caching here once it is working correctly.
# if 1
        NodeKey key = node_key(e);
        std::map<NodeKey, InfInterval>::const_iterator found = interval_cache.find(key);
        if (found != interval_cache.end()) {
            return found->second;
        }
# endif
        process(e); 
        //interval_cache[key] = interval;
        return interval; 
    }

private:
    InfInterval bounds_of_type(Type t) {
        if (t.is_uint()) {
            if (t.bits <= 31) { // Previously, more than 16 bits was unbounded, but in practice that meant 32
                return InfInterval(t.min(), t.max());
            } else {
                // Hack: Treat 32 bit unsigned int as unbounded.
                // Previously, min was undef, but zero is sensible.
                return InfInterval(t.min(), new Infinity(t, 1));
            }
        } else if (t.is_int()) {
            if (t.bits <= 31) { // Previously, more than 16 bbits was unbounded, but in practice that meant 32
                return InfInterval(t.min(), t.max());
            } else {
                // Hack: Treat 32 bit signed integer as unbounded.
                return InfInterval(new Infinity(t, -1), new Infinity(t, 1));
            }
            
        } else {
            // Floating point types are treated as unbounded for analysis
            return InfInterval(new Infinity(t, -1), new Infinity(t, 1));
        }        
    }

protected:
    using IRLazyScope<IRProcess>::visit;
    
    virtual void visit(const IntImm *op) {
        interval = InfInterval(op, op);
    }
    
    virtual void visit(const FloatImm *op) {
        interval = InfInterval(op, op);
    }

    virtual void visit(const Cast *op) {
        // Assume no overflow
        InfInterval value = bounds(op->value);
        interval = InfInterval(new Cast(op->type, value.min), new Cast(op->type, value.max));
    }

    virtual void visit(const Variable *op) {
        // Encountered a variable.  Try interval analysis on it.
        int found = find_variable(op->name);
        //log(0) << "IA find " << op->name << " in context " << current_context() << ": " << found << "\n";
        if (found) {
            const DefiningNode *def = call(found);
            // Now we are in the defining node context.
            const For *def_for = def->node().as<For>();
            const Let *def_let = def->node().as<Let>();
            const LetStmt *def_letstmt = def->node().as<LetStmt>();
            if (def_for) {
                // Defined as a For loop.  The interval is easily determined.
                //log(0) << "    for loop in context " << current_context() << "\n";
                //log(0) << "        interval of " << def_for->min << "\n";
                InfInterval formin = bounds(def_for->min);
                Expr emax = def_for->min + (def_for->extent - 1);
                //log(0) << "        interval of " << emax << "\n";
                InfInterval formax = bounds(emax);
                //log(0) << "    min: " << formin << " " << def_for->min << "\n";
                //log(0) << "    max: " << formax << " " << emax << "\n";
                interval = InfInterval(formin.min, formax.max);
            } else if (def_let) {
                //log(0) << "    let\n";
                interval = bounds(def_let->value);
            } else if (def_letstmt) {
                //log(0) << "    letstmt " << op->name << "\n";
                interval = bounds(def_letstmt->value);
            } else {
                assert(0 && "Unknown defining node for variable");
                interval = InfInterval(new Infinity(-1), new Infinity(1));
            }
            log(LOGLEVEL) << "bounds(" << op->name << "): " << interval << "\n";
            ret(found);
        } else {
            // Keep the variable name in the absence of a definition.
            // This allows symbolic reasoning (i.e. simplify) to reduce
            // expressions.
            interval = InfInterval(op, op);
        }
    }
    
    virtual void visit(const Add *op) {
        interval = bounds(op->a) + bounds(op->b);
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }
    
    virtual void visit(const Sub *op) {
        interval = bounds(op->a) - bounds(op->b);
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }
    
    virtual void visit(const Mul *op) {
        interval = bounds(op->a) * bounds(op->b);
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }
    
    virtual void visit(const Div *op) {
        interval = bounds(op->a) / bounds(op->b);
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }
    
    virtual void visit(const Mod *op) {
        interval = bounds(op->a) % bounds(op->b);
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }
    
    virtual void visit(const Min *op) {
        interval = min(bounds(op->a), bounds(op->b));
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }
    
    virtual void visit(const Max *op) {
        interval = max(bounds(op->a), bounds(op->b));
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }
    
    virtual void visit(const Clamp *op) {
        // Expression a is clamped to (op->min, op->max)
        // The clamping interval is the union of the intervals of op->min and op->max.
        // The resulting interval is no larger than the clamping interval and also no larger
        // than the interval of expression a.
        interval = intersection(bounds(op->a), 
                                infinterval_union(bounds(op->min), bounds(op->max)));
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }
    
    virtual void visit(const EQ *op) {
        InfInterval a = bounds(op->a);
        InfInterval b = bounds(op->b);
        // Any variables in the interval expressions cannot be
        // further resolved, so there is no need for the current
        // variable bindings to be used by proved() below.
        if (proved(a.max < b.min) || proved(a.min > b.max)) {
            // The intervals do not overlap. Equality is disproved.
            interval = InfInterval(const_false(op->type.width), const_false(op->type.width));
        } else if (equal(a.min, a.max) && equal(a.min, b.min) && equal(a.min, b.max)) {
            // Both intervals are unique constants and equal.
            interval = InfInterval(const_true(op->type.width), const_true(op->type.width));
        } else {
            interval = InfInterval(const_false(op->type.width), const_true(op->type.width));
        }
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }
    
    virtual void visit(const NE *op) {
        interval = bounds(! (op->a == op->b));
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }
    
    virtual void visit(const LT *op) {
        InfInterval a = bounds(op->a);
        InfInterval b = bounds(op->b);
        if (proved(a.max < b.min)) {
            // a is always less than b
            interval = InfInterval(const_true(op->type.width), const_true(op->type.width));
        } else if (proved(a.min >= b.max)) {
            // a is never less than b.
            interval = InfInterval(const_false(op->type.width), const_false(op->type.width));
        } else {
            interval = InfInterval(const_false(op->type.width), const_true(op->type.width));
        }
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }
    
    virtual void visit(const LE *op) {
        InfInterval a = bounds(op->a);
        InfInterval b = bounds(op->b);
        if (proved(a.max <= b.min)) {
            // a is always <= b
            interval = InfInterval(const_true(op->type.width), const_true(op->type.width));
        } else if (proved(a.min > b.max)) {
            // a is never <= b.
            interval = InfInterval(const_false(op->type.width), const_false(op->type.width));
        } else {
            interval = InfInterval(const_false(op->type.width), const_true(op->type.width));
        }
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }
    
    virtual void visit(const GT *op) {
        interval = bounds(op->b < op->a);
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }
    
    virtual void visit(const GE *op) {
        interval = bounds(op->b <= op->a);
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }
    
    virtual void visit(const And *op) {
        InfInterval a = bounds(op->a);
        InfInterval b = bounds(op->b);
        if (is_zero(a.max)) {
            // If one is proved false, then it is the result
            interval = a;
        } else if (is_zero(b.max)) {
            interval = b;
        } else if (is_one(a.min)) {
            // If one is proved true, then the other is the result.
            interval = b;
        } else if (is_one(b.min)) {
            interval = a;
        } else {
            // Neither is proved true nor proved false.
            // The result is uncertain.
            interval = InfInterval(const_false(op->type.width), const_true(op->type.width));
        }
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }
    
    virtual void visit(const Or *op) {
        InfInterval a = bounds(op->a);
        InfInterval b = bounds(op->b);
        if (is_one(a.min)) {
            // If one is proved true, then it is the result
            interval = a;
        } else if (is_one(b.min)) {
            interval = b;
        } else if (is_zero(a.max)) {
            // If one is proved false, then the other is the result.
            interval = b;
        } else if (is_zero(b.max)) {
            interval = a;
        } else {
            // Neither is proved true nor proved false.
            // The result is uncertain.
            interval = InfInterval(const_false(op->type.width), const_true(op->type.width));
        }
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }
    
    virtual void visit(const Not *op) {
        InfInterval a = bounds(op->a);
        if (is_one(a.min)) {
            // If a is proved true, then the result is false
            interval = InfInterval(const_false(op->type.width), const_false(op->type.width));
        } else if (is_zero(a.max)) {
            // If a is proved false, then the result is true.
            interval = InfInterval(const_true(op->type.width), const_true(op->type.width));
        } else {
            // Neither proved true nor proved false.
            // The result is uncertain.
            interval = InfInterval(const_false(op->type.width), const_true(op->type.width));
        }
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }
    
    virtual void visit(const Select *op) {
        InfInterval condition = bounds(op->condition);
        if (is_one(condition.min)) {
            // If condition is proved true, then the result is always the true_value
            interval = bounds(op->true_value);
        } else if (is_zero(condition.max)) {
            // If condition is proved false, then the result is always the false_value
            interval = bounds(op->false_value);
        } else {
            // Neither proved true nor proved false.
            // The result can be either expressions.
            interval = infinterval_union(bounds(op->true_value),
                                  bounds(op->false_value));
        }
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }
    
    virtual void visit(const Load *op) {
        // Here we could do better than the bounds of the type if we know what computed
        // the data that we are loading - i.e. if it comes from another function.
        interval = bounds_of_type(op->type);
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }

    virtual void visit(const Ramp *op) {
        InfInterval base = bounds(op->base);
        InfInterval stride = bounds(op->stride);
        
        // Here we return a ramp representing the interval of values in each position of the
        // interpreted Ramp node.
        interval = InfInterval(new Ramp(base.min, stride.min, op->width), new Ramp(base.max, stride.max, op->width));
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }

    virtual void visit(const Broadcast *op) {
        InfInterval value = bounds(op->value);
        interval = InfInterval(new Broadcast(value.min, op->width), new Broadcast(value.max, op->width));
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }

    virtual void visit(const Solve *op) {
        interval = bounds(op->body);
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }

    virtual void visit(const TargetVar *op) {
        interval = bounds(op->body);
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }

    virtual void visit(const Call *op) {

        // Could surely do better than simply take the bounds of the type.
        // The full solution for interval analysis of the current
        // expression involves interval analysis on the called function.
        interval = bounds_of_type(op->type);
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }

    virtual void visit(const Let *op) {
        interval = bounds(op->body);
        log(LOGLEVEL) << "bounds(" << Expr(op) << "): " << interval << "\n";
    }
    
    virtual void visit(const LetStmt *op) {
        interval = InfInterval(); 
    }

    virtual void visit(const PrintStmt *op) {
        interval = InfInterval(); 
    }

    virtual void visit(const AssertStmt *op) {
        interval = InfInterval(); 
    }

    virtual void visit(const Pipeline *op) {
        interval = InfInterval(); 
    }

    virtual void visit(const For *op) {
        interval = InfInterval(); 
    }

    virtual void visit(const Store *op) {
        interval = InfInterval(); 
    }

    virtual void visit(const Provide *op) {
        interval = InfInterval(); 
    }

    virtual void visit(const Allocate *op) {
        interval = InfInterval(); 
    }

    virtual void visit(const Realize *op) {
        interval = InfInterval(); 
    }

    virtual void visit(const Block *op) {
        interval = InfInterval(); 
    }
    
    virtual void visit(const StmtTargetVar *op) {
        interval = InfInterval(); 
    }
    
    virtual void visit(const Infinity *op) {
        // Warning: If interval analysis is used in Simplify then
        // it may result in simplify trying to perform interval analysis
        // on interval expressions that contain infinity.
        // That would be a real problem - we dont want recursive analysis
        // of interval expressions by interval analysis.
        // Simplification that incorporates interval analysis needs to be
        // kept separate.
        assert(0 && "Infinity node found in parse tree by interval analysis"); 
    }
};


// end namespace Internal
}
}

# endif
