#ifndef HALIDE_INTERVALANAL_H
#define HALIDE_INTERVALANAL_H

#include "IR.h"
#include "Ival.h"
#include <map>

namespace Halide { 
namespace Internal {

using std::string;

/** Class that performs interval analysis using IRLazyScope.
 * This class can be called in the middle of a mutation or
 * other tree walk and will compute the interval analysis result
 * for the current tree node.  Note that this may involve
 * walking the tree, and IRLazyScope will be used to track
 * variable bindings and contexts while the interval analysis
 * is being performed.  This may result in information being cached,
 * but that it why we use IRLazyScope and the ContextManager.
 *
 * This class uses Ival to represent the interval, with infinity
 * for unbounded intervals.
 */

class IntervalAnal : public IRLazyScope<IRProcess> {
    Ival interval;
    
    // Ideally the interval cache would be static and shared
    // in the same way as ContextManager.  It would then
    // want to be reset whenever the context manager is reset.
    // To do this in a tidy manner is difficult.  The untidy way
    // is to include it in the context manager.
    std::map<NodeKey, Ival> interval_cache;
    
public:
    Ival interval_analysis(Expr e) { 
        // Insert caching here once it is working correctly.
        NodeKey key = node_key(e);
        std::map<NodeKey, Ival>::const_iterator found = interval_cache.find(key);
        if (found != interval_cache.end()) {
            return found->second;
        }
        process(e); 
        interval_cache[key] = interval;
        return interval; 
    }

private:
    Ival bounds_of_type(Type t) {
        if (t.is_uint()) {
            if (t.bits <= 31) { // Previously, more than 16 bits was unbounded, but in practice that meant 32
                return Ival(t.min(), t.max());
            } else {
                // Hack: Treat 32 bit unsigned int as unbounded.
                // Previously, min was undef, but zero is sensible.
                return Ival(t.min(), new Infinity(t, 1));
            }
        } else if (t.is_int()) {
            if (t.bits <= 31) { // Previously, more than 16 bbits was unbounded, but in practice that meant 32
                return Ival(t.min(), t.max());
            } else {
                // Hack: Treat 32 bit signed integer as unbounded.
                return Ival(new Infinity(t, -1), new Infinity(t, 1));
            }
            
        } else {
            // Floating point types are treated as unbounded for analysis
            return Ival(new Infinity(t, -1), new Infinity(t, 1));
        }        
    }

protected:
    using IRLazyScope<IRProcess>::visit;
    
    virtual void visit(const IntImm *op) {
        interval = Ival(op, op);
    }
    
    virtual void visit(const FloatImm *op) {
        interval = Ival(op, op);
    }

    virtual void visit(const Cast *op) {
        // Assume no overflow
        Ival value = interval_analysis(op->value);
        interval = Ival(new Cast(op->type, value.min), new Cast(op->type, value.max));
    }

    virtual void visit(const Variable *op) {
        // Encountered a variable.  Try interval analysis on it.
        int found = find_variable(op->name);
        if (found) {
            const DefiningNode *def = call(found);
            // Now we are in the defining node context.
            const For *def_for = def->node().as<For>();
            const Let *def_let = def->node().as<Let>();
            const LetStmt *def_letstmt = def->node().as<LetStmt>();
            if (def_for) {
                // Defined as a For loop.  The interval is easily determined.
                interval = Ival(def_for->min, def_for->min + def_for->extent - 1);
            } else if (def_let) {
                interval = interval_analysis(def_let->value);
            } else if (def_letstmt) {
                interval = interval_analysis(def_letstmt->value);
            } else {
                interval = Ival(new Infinity(-1), new Infinity(1));
            }
            ret(found);
        } else {
            interval = Ival(op, op); // In the absence of a definition, keep the variable name.
        }
    }
    
    virtual void visit(const Add *op) {
        interval = interval_analysis(op->a) + interval_analysis(op->b);
    }
    
    virtual void visit(const Sub *op) {
        interval = interval_analysis(op->a) - interval_analysis(op->b);
    }
    
    virtual void visit(const Mul *op) {
        interval = interval_analysis(op->a) * interval_analysis(op->b);
    }
    
    virtual void visit(const Div *op) {
        interval = interval_analysis(op->a) / interval_analysis(op->b);
    }
    
    virtual void visit(const Mod *op) {
        interval = interval_analysis(op->a) % interval_analysis(op->b);
    }
    
    virtual void visit(const Min *op) {
        interval = min(interval_analysis(op->a), interval_analysis(op->b));
    }
    
    virtual void visit(const Max *op) {
        interval = max(interval_analysis(op->a), interval_analysis(op->b));
    }
    
    virtual void visit(const Clamp *op) {
        // Expression a is clamped to (op->min, op->max)
        // The clamping interval is the union of the intervals of op->min and op->max.
        // The resulting interval is no larger than the clamping interval and also no larger
        // than the interval of expression a.
        interval = intersection(interval_analysis(op->a), 
                                ival_union(interval_analysis(op->min), interval_analysis(op->max)));
    }
    
    virtual void visit(const EQ *op) {
        Ival a = interval_analysis(op->a);
        Ival b = interval_analysis(op->b);
        if (proved(a.max < b.min) || proved(a.min > b.max)) {
            // The intervals do not overlap. Equality is disproved.
            interval = Ival(const_false(op->type.width), const_false(op->type.width));
        } else if (equal(a.min, a.max) && equal(a.min, b.min) && equal(a.min, b.max)) {
            // Both intervals are unique constants and equal.
            interval = Ival(const_true(op->type.width), const_true(op->type.width));
        } else {
            interval = Ival(const_false(op->type.width), const_true(op->type.width));
        }
    }
    
    virtual void visit(const NE *op) {
        interval = interval_analysis(! (op->a == op->b));
    }
    
    virtual void visit(const LT *op) {
        Ival a = interval_analysis(op->a);
        Ival b = interval_analysis(op->b);
        if (proved(a.max < b.min)) {
            // a is always less than b
            interval = Ival(const_true(op->type.width), const_true(op->type.width));
        } else if (proved(a.min >= b.max)) {
            // a is never less than b.
            interval = Ival(const_false(op->type.width), const_false(op->type.width));
        } else {
            interval = Ival(const_false(op->type.width), const_true(op->type.width));
        }
    }
    
    virtual void visit(const LE *op) {
        Ival a = interval_analysis(op->a);
        Ival b = interval_analysis(op->b);
        if (proved(a.max <= b.min)) {
            // a is always <= b
            interval = Ival(const_true(op->type.width), const_true(op->type.width));
        } else if (proved(a.min > b.max)) {
            // a is never <= b.
            interval = Ival(const_false(op->type.width), const_false(op->type.width));
        } else {
            interval = Ival(const_false(op->type.width), const_true(op->type.width));
        }
    }
    
    virtual void visit(const GT *op) {
        interval = interval_analysis(op->b < op->a);
    }
    
    virtual void visit(const GE *op) {
        interval = interval_analysis(op->b <= op->a);
    }
    
    virtual void visit(const And *op) {
        Ival a = interval_analysis(op->a);
        Ival b = interval_analysis(op->b);
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
            interval = Ival(const_false(op->type.width), const_true(op->type.width));
        }
    }
    
    virtual void visit(const Or *op) {
        Ival a = interval_analysis(op->a);
        Ival b = interval_analysis(op->b);
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
            interval = Ival(const_false(op->type.width), const_true(op->type.width));
        }
    }
    
    virtual void visit(const Not *op) {
        Ival a = interval_analysis(op->a);
        if (is_one(a.min)) {
            // If a is proved true, then the result is false
            interval = Ival(const_false(op->type.width), const_false(op->type.width));
        } else if (is_zero(a.max)) {
            // If a is proved false, then the result is true.
            interval = Ival(const_true(op->type.width), const_true(op->type.width));
        } else {
            // Neither proved true nor proved false.
            // The result is uncertain.
            interval = Ival(const_false(op->type.width), const_true(op->type.width));
        }
    }
    
    virtual void visit(const Select *op) {
        Ival condition = interval_analysis(op->condition);
        if (is_one(condition.min)) {
            // If condition is proved true, then the result is always the true_value
            interval = interval_analysis(op->true_value);
        } else if (is_zero(condition.max)) {
            // If condition is proved false, then the result is always the false_value
            interval = interval_analysis(op->false_value);
        } else {
            // Neither proved true nor proved false.
            // The result can be either expressions.
            interval = ival_union(interval_analysis(op->true_value),
                                  interval_analysis(op->false_value));
        }
    }
    
    virtual void visit(const Load *op) {
        // Here we could do better than the bounds of the type if we know what computed
        // the data that we are loading - i.e. if it comes from another function.
        interval = bounds_of_type(op->type);
    }

    virtual void visit(const Ramp *op) {
        Ival base = interval_analysis(op->base);
        Ival stride = interval_analysis(op->stride);
        
        // Here we return a ramp representing the interval of values in each position of the
        // interpreted Ramp node.
        interval = Ival(new Ramp(base.min, stride.min, op->width), new Ramp(base.max, stride.max, op->width));
    }

    virtual void visit(const Broadcast *op) {
        Ival value = interval_analysis(op->value);
        interval = Ival(new Broadcast(value.min, op->width), new Broadcast(value.max, op->width));
    }

    virtual void visit(const Solve *op) {
        interval = interval_analysis(op->body);
    }

    virtual void visit(const TargetVar *op) {
        interval = interval_analysis(op->body);
    }

    virtual void visit(const Call *op) {

        // Could surely do better than simply take the bounds of the type.
        // The full solution for interval analysis of the current
        // expression involves interval analysis on the called function.
        interval = bounds_of_type(op->type);
    }

    virtual void visit(const Let *op) {
        interval = interval_analysis(op->body);
    }
    
    virtual void visit(const LetStmt *op) {
        interval = Ival(); 
    }

    virtual void visit(const PrintStmt *op) {
        interval = Ival(); 
    }

    virtual void visit(const AssertStmt *op) {
        interval = Ival(); 
    }

    virtual void visit(const Pipeline *op) {
        interval = Ival(); 
    }

    virtual void visit(const For *op) {
        interval = Ival(); 
    }

    virtual void visit(const Store *op) {
        interval = Ival(); 
    }

    virtual void visit(const Provide *op) {
        interval = Ival(); 
    }

    virtual void visit(const Allocate *op) {
        interval = Ival(); 
    }

    virtual void visit(const Realize *op) {
        interval = Ival(); 
    }

    virtual void visit(const Block *op) {
        interval = Ival(); 
    }
    
    virtual void visit(const StmtTargetVar *op) {
        interval = Ival(); 
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
