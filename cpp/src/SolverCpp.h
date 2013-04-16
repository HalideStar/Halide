#include "Solver.h"

namespace Halide { 
namespace Internal {

using std::string;


// Solve performs backwards interval analysis.
// A solution is of the form a <= e <= b.
// Solve derives a vector of solutions.
// To define what is interesting to you, override specific visit methods in
// a class defined from Solve. (That means that, for now, your code has to be
// included in this file.)
// Solve knows how to update the vector of solutions for common expression
// nodes.

// Solve needs to be able to simplify expressions, but it needs to
// simplify them with respect to a set of variables.
// To implement this capability, we need to override some of the behaviour of
// Simplify. We cannot do that by calling simplify(), so intead we base Solve on
// Simplify and override the visit methods.

// Unfortunately, Simplify is not a public class, so Solve ends up being included
// in Simplify.cpp.

class Solution {
    // The expression that we have solved for.  Will contain target variable(s).
    // A good solution is one where the solved expression is a single variable.
    Expr e;
    
    // Intervals that define the individual solutions.
    std::vector<Interval> intervals;
};
    


// HasVariable walks an argument expression and determines
// whether the expression contains any of the listed variables.

class HasVariable : public IRVisitor {
private:
    const std::vector<std::string> &varlist;

public:
    bool result;
    HasVariable(const std::vector<std::string> &variables) : varlist(variables), result(false) {}
    
private:
    using IRVisitor::visit;

    void visit(const Variable *op) {
        if (result) return; // Once one is found, no need to find more.
        // Check whether variable name is in the list of known names
        for (size_t i = 0; i < varlist.size(); i++) {
            if (varlist[i] == op->name) {
                result = true;
                break;
            }
        }
    }
};

// is_constant_expr: Determine whether an expression is constant relative to a list of free variables
// that may not occur in the expression.
bool is_constant_expr(std::vector<std::string> varlist, Expr e) {
    // Walk the expression; if no variables in varlist are found then it is a constant expression
    HasVariable hasvar(varlist);
    e.accept(&hasvar);
    return ! hasvar.result;
}

namespace {
// Convenience methods for building solve nodes.
Expr solve(Expr e, Interval i) {
    return new Solve(e, i);
}

Expr solve(Expr e, std::vector<Interval> i) {
    return new Solve(e, i);
}

// Apply unary operator to a vector of Interval by applying it to each Interval
inline std::vector<Interval> v_apply(Interval (*f)(Interval), std::vector<Interval> v) {
    std::vector<Interval> result;
    for (size_t i = 0; i < v.size(); i++) {
        result.push_back((*f)(v[i]));
    }
    return result;
}

// Apply binary operator to a vector of Interval by applying it to each Interval
inline std::vector<Interval> v_apply(Interval (*f)(Interval, Expr), std::vector<Interval> v, Expr b) {
    std::vector<Interval> result;
    for (size_t i = 0; i < v.size(); i++) {
        result.push_back((*f)(v[i], b));
    }
    return result;
}
}

class Solver : public Simplify {

    // using parent::visit indicates that
    // methods of the base class with different parameters
    // but the same name are not hidden, they are overloaded.
    using Simplify::visit;
    
public:
    
    // The target variables for solving. (A scope-like thing)
    std::vector<std::string> targets;
    
    bool is_constant_expr(Expr e) {
        return Halide::Internal::is_constant_expr(targets, e);
    }
    
    
    void visit(const Solve *op) {
        log(0) << depth << " Solve simplify " << Expr(op) << "\n";
        Expr e = mutate(op->e);
        
        const Add *add_e = e.as<Add>();
        const Sub *sub_e = e.as<Sub>();
        const Mul *mul_e = e.as<Mul>();
        const Div *div_e = e.as<Div>();
        
        if (add_e && is_constant_expr(add_e->b)) {
            expr = mutate(solve(add_e->a, v_apply(operator-, op->v, add_e->b)) + add_e->b);
        } else if (sub_e && is_constant_expr(sub_e->b)) {
            expr = mutate(solve(sub_e->a, v_apply(operator+, op->v, sub_e->b)) - sub_e->b);
        } else if (sub_e && is_constant_expr(sub_e->a)) {
            // solve(k - v) --> -solve(v - k) with interval negated
            expr = mutate(-solve(sub_e->b - sub_e->a, v_apply(operator-, op->v)));
        } else if (mul_e && is_constant_expr(mul_e->b)) {
            // solve(v * k) on (a,b) --> solve(v) * k with interval (ceil(a/k), floor(b/k))
            expr = mutate(solve(mul_e->a, v_apply(operator/, op->v, mul_e->b)) * mul_e->b);
        } else if (div_e && is_constant_expr(div_e->b)) {
            // solve(v / k) on (a,b) --> solve(v) / k with interval a * k, b * k + (k +/- 1)
            expr = mutate(solve(div_e->a, v_apply(operator*, op->v, div_e->b)) / div_e->b);
        } else {
            expr = op; // Nothing more to do.
        }
        log(0) << depth << " Solve simplified to " << expr << "\n";
    }
    
    
    //void visit(const IntImm *op) {
    //void visit(const FloatImm *op) {
    //void visit(const Cast *op) {
    //void visit(const Variable *op) {
    
    void visit(const Add *op) {
        log(0) << depth << " XAdd simplify " << Expr(op) << "\n";
        Expr a = mutate(op->a), b = mutate(op->b);
        
        // Override default behavior: any constant expression is pushed outside of variable expressions.
        if (is_constant_expr(a) && ! is_constant_expr(b)) {
            std::swap(a, b);
        }
        
        
        const Add *add_a = a.as<Add>();
        const Add *add_b = b.as<Add>();
        const Sub *sub_a = a.as<Sub>();

        // The default behavior of simplify pushes constant values towards the RHS.
        // We want to preserve that behavior because it results in constants being
        // simplified.  On top of that, however, we want to push constant variables
        // and expressions consisting only of constant variables outside of expressions
        // consisting of/containing target variables.
        
        // In the following comments, k... denotes constant expression, v... denotes target variable.
        if (is_constant_expr(a) && is_constant_expr(b)) {
            Simplify::visit(op); // Pure constant expressions get simplified in the normal way
        } else if (add_a && is_constant_expr(add_a->b) && ! is_constant_expr(b)) {
            // (aa + kab) + vb --> (aa + vb) + kab
            expr = mutate((add_a->a + b) + add_a->b);
        } else if (add_b && is_constant_expr(add_b->b) && !is_constant_expr(add_b->a)) {
            // a + (vba + kbb) --> (a + vba) + kbb
            expr = mutate((a + add_b->a) + add_b->b);
        } else if (sub_a && is_constant_expr(sub_a->a) && is_constant_expr(b)) {
            // (kaa - ab) + kb --> (kaa + kb) - ab
            expr = mutate((sub_a->a + b) - sub_a->b);
        } else {
            // Adopt default behavior
            // Take care in the above rules to ensure that they do not produce changes that
            // are reversed by Simplify::visit(Add)
            Simplify::visit(op);
        }
        log(0) << depth << " XAdd simplified to " << expr << "\n";
    }

    void visit(const Sub *op) {
        log(0) << depth << " XSub simplify " << Expr(op) << "\n";
        Expr a = mutate(op->a), b = mutate(op->b);
        
        // Override default behavior.
        // Push constant expressions outside of variable expressions.
        
        const Add *add_a = a.as<Add>();
        const Add *add_b = b.as<Add>();
        const Sub *sub_a = a.as<Sub>();
        const Sub *sub_b = b.as<Sub>();
        
        // In the following comments, k... denotes constant expression, v... denotes target variable.
        if (is_constant_expr(a) && is_constant_expr(b)) {
            Simplify::visit(op); // Pure constant expressions get simplified in the normal way
        } else if (add_a && is_constant_expr(add_a->b) && ! is_constant_expr(b) & !is_constant_expr(add_a->a)) {
            // (vaa + kab) - kb --> unchanged (because other simplify rules would reverse)
            // (vaa + kab) - vb --> (vaa - vb) + kab
            expr = mutate((add_a->a - b) + add_a->b);
        } else if (add_b && is_constant_expr(add_b->b) && !is_constant_expr(add_b->a)) {
            // a - (vba + kbb) --> (a - vba) - kbb
            expr = mutate((a - add_b->a) - add_b->b);
        } else if (sub_a && is_constant_expr(sub_a->a) && is_constant_expr(b)) {
            // (kaa - ab) - kb --> (kaa - kb) - ab
            expr = mutate((sub_a->a - b) - sub_a->b);
        } else if (sub_b && is_constant_expr(sub_b->b)) {
            // Unused: ka - (ba - kbb) --> (ka + kbb) - ba: Such a rule is reversed by simplify
            if (is_constant_expr(a)) expr = mutate((a + sub_b->b) - sub_b->a);
            // a - (ba - kbb) --> (a - ba) + kbb;
            else expr = mutate((a - sub_b->a) + sub_b->b);
        } else if (sub_b && is_constant_expr(sub_b->a) && is_constant_expr(a)) {
            // ka - (kba - bb) --> (bb + ka) - kba
            if (is_constant_expr(a)) expr = mutate((sub_b->b + a) - sub_b->a);
            // a - (kba - bb) --> (a + bb) - kba
            else expr = mutate((a + sub_b->b) - sub_b->a);
        } else {
            // Adopt default behavior
            // Take care in the above rules to ensure that they do not produce changes that
            // are reversed by Simplify::visit(Add)
            Simplify::visit(op);
        }
        log(0) << depth << " XSub simplified to " << expr << "\n";
    }

    void visit(const Mul *op) {
        log(0) << depth << " XMul simplify " << Expr(op) << "\n";
        Expr a = mutate(op->a), b = mutate(op->b);

        // Override default behavior: any constant expression is pushed outside of variable expressions.
        if (is_constant_expr(a) && ! is_constant_expr(b)) {
            std::swap(a, b);
        }
        
        //const Add *add_a = a.as<Add>();
        const Mul *mul_a = a.as<Mul>();
        
        //if (add_a && is_constant_expr(add_a->b) && is_constant_expr(b)) {
            // (aa + kab) * kb --> (aa * kb) + (kab + kb)
            //expr = mutate(add_a->a * b + add_a->b * b);
        //} else 
        if (mul_a && is_constant_expr(mul_a->b) && is_constant_expr(b)) {
            // (aa * kab) * kb --> aa * (kab * kb)
            expr = mutate(mul_a->a * (mul_a->b * b));
        //} else if (mul_b && is_constant_expr(b)) {
            // aa * kb: Keep unchanged.
            //expr = op;
        } else {
            Simplify::visit(op);
        }
        log(0) << depth << " XMul simplified to " << expr << "\n";
    }

# if 0
    void visit(const Div *op) {
        Expr a = mutate(op->a), b = mutate(op->b);
        
        int ia, ib;
        float fa, fb;

        const Mul *mul_a = a.as<Mul>();
        const Add *add_a = a.as<Add>();
        const Sub *sub_a = a.as<Sub>();
        const Div *div_a = a.as<Div>();
        const Mul *mul_a_a = NULL;
        const Mul *mul_a_b = NULL;
        const Broadcast *broadcast_a = a.as<Broadcast>();
        const Ramp *ramp_a = a.as<Ramp>();
        const Broadcast *broadcast_b = b.as<Broadcast>();

        if (add_a) {
            mul_a_a = add_a->a.as<Mul>();
            mul_a_b = add_a->b.as<Mul>();
        } else if (sub_a) {
            mul_a_a = sub_a->a.as<Mul>();
            mul_a_b = sub_a->b.as<Mul>();
        }

        if (is_zero(a)) {
            expr = a;
        } else if (is_one(b)) {
            expr = a;
        } else if (equal(a, b)) {
            expr = make_one(a.type());
        } else if (const_int(a, &ia) && const_int(b, &ib)) {
            expr = div_imp(ia,ib);
        } else if (const_float(a, &fa) && const_float(b, &fb)) {
            expr = fa/fb;
        } else if (const_castint(a, &ia) && const_castint(b, &ib)) {
            if (op->type.is_uint()) {
                expr = make_const(op->type, ((unsigned int)ia)/((unsigned int)ib));
            } else {
                expr = make_const(op->type, div_imp(ia,ib)); //Use the new definition of division
            }
        } else if (broadcast_a && broadcast_b) {
            expr = mutate(new Broadcast(broadcast_a->value / broadcast_b->value, broadcast_a->width));
        } else if (ramp_a && broadcast_b && 
                   const_int(broadcast_b->value, &ib) && 
                   const_int(ramp_a->stride, &ia) && ((ia % ib) == 0)) {
            // ramp(x, ia, w) / broadcast(ib, w) -> ramp(x/ib, ia/ib, w) when ib divides ia
            expr = mutate(new Ramp(ramp_a->base/ib, ia/ib, ramp_a->width));
        } else if (div_a && const_int(div_a->b, &ia) && const_int(b, &ib)) {
            // (x / 3) / 4 -> x / 12
            expr = mutate(div_a->a / (ia*ib));
        } else if (mul_a && const_int(mul_a->b, &ia) && const_int(b, &ib) && 
                   ia && ib && (ia % ib == 0 || ib % ia == 0)) {
            if (ia % ib == 0) {
                // (x * 4) / 2 -> x * 2
                expr = mutate(mul_a->a * (ia / ib));
            } else {
                // (x * 2) / 4 -> x / 2
                expr = mutate(mul_a->a / (ib / ia));
            }            
        } else if (add_a && mul_a_a && const_int(mul_a_a->b, &ia) && const_int(b, &ib) && 
                   ib && (ia % ib == 0)) {
            // Pull terms that are a multiple of the divisor out
            // (x*4 + y) / 2 -> x*2 + y/2            
            expr = mutate((mul_a_a->a * (ia/ib)) + (add_a->b / b));
        } else if (add_a && mul_a_b && const_int(mul_a_b->b, &ia) && const_int(b, &ib) && 
                   ib && (ia % ib == 0)) {
            // (y + x*4) / 2 -> y/2 + x*2
            expr = mutate((add_a->a / b) + (mul_a_b->a * (ia/ib)));
        } else if (sub_a && mul_a_a && const_int(mul_a_a->b, &ia) && const_int(b, &ib) && 
                   ib && (ia % ib == 0)) {
            // Pull terms that are a multiple of the divisor out
            // (x*4 - y) / 2 -> x*2 - y/2            
            expr = mutate((mul_a_a->a * (ia/ib)) - (sub_a->b / b));
        } else if (sub_a && mul_a_b && const_int(mul_a_b->b, &ia) && const_int(b, &ib) && 
                   ib && (ia % ib == 0)) {
            // (y - x*4) / 2 -> y/2 - x*2
            expr = mutate((sub_a->a / b) - (mul_a_b->a * (ia/ib)));
        } else if (b.type().is_float() && is_simple_const(b)) {
            // Convert const float division to multiplication
            // x / 2 -> x * 0.5
            expr = mutate(a * (make_one(b.type()) / b));
        } else if (a.same_as(op->a) && b.same_as(op->b)) {
            expr = op;
        } else {
            expr = new Div(a, b);
        }
    }

    void visit(const Mod *op) {
        Expr a = mutate(op->a), b = mutate(op->b);

        int ia, ib;
        float fa, fb;
        const Broadcast *broadcast_a = a.as<Broadcast>();
        const Broadcast *broadcast_b = b.as<Broadcast>();
        const Mul *mul_a = a.as<Mul>();
        const Add *add_a = a.as<Add>();
        const Mul *mul_a_a = add_a ? add_a->a.as<Mul>() : NULL;
        const Mul *mul_a_b = add_a ? add_a->b.as<Mul>() : NULL;
        const Ramp *ramp_a = a.as<Ramp>();

        // If the RHS is a constant, do modulus remainder analysis on the LHS
        ModulusRemainder mod_rem(0, 1);
        if (const_int(b, &ib) && a.type() == Int(32)) {
            mod_rem = modulus_remainder(a, alignment_info);
        }

        if (const_int(a, &ia) && const_int(b, &ib)) {
            expr = mod_imp(ia, ib);
        } else if (const_float(a, &fa) && const_float(b, &fb)) {
            expr = mod_imp(fa, fb);
        } else if (const_castint(a, &ia) && const_castint(b, &ib)) {
            if (op->type.is_uint()) {
                expr = make_const(op->type, ((unsigned int)ia) % ((unsigned int)ib));
            } else {
                expr = new Cast(op->type, mod_imp(ia, ib));
            }
        } else if (broadcast_a && broadcast_b) {
            expr = mutate(new Broadcast(broadcast_a->value % broadcast_b->value, broadcast_a->width));
        } else if (mul_a && const_int(b, &ib) && const_int(mul_a->b, &ia) && (ia % ib == 0)) {
            // (x * (b*a)) % b -> 0
            expr = make_zero(a.type());
        } else if (add_a && mul_a_a && const_int(mul_a_a->b, &ia) && const_int(b, &ib) && (ia % ib == 0)) {
            // (x * (b*a) + y) % b -> (y % b)
            expr = mutate(add_a->b % ib);
        } else if (add_a && mul_a_b && const_int(mul_a_b->b, &ia) && const_int(b, &ib) && (ia % ib == 0)) {
            // (y + x * (b*a)) % b -> (y % b)
            expr = mutate(add_a->a % ib);
        } else if (const_int(b, &ib) && a.type() == Int(32) && mod_rem.modulus % ib == 0) {
            // ((a*b)*x + c) % a -> c % a
            expr = mod_rem.remainder % ib;
        } else if (ramp_a && const_int(ramp_a->stride, &ia) && 
                   broadcast_b && const_int(broadcast_b->value, &ib) &&
                   ia % ib == 0) {
            // ramp(x, 4, w) % broadcast(2, w)
            expr = mutate(new Broadcast(ramp_a->base % ib, ramp_a->width));
        } else if (a.same_as(op->a) && b.same_as(op->b)) {
            expr = op;
        } else {
            expr = new Mod(a, b);            
        }
    }

    // Utility routine to compare ramp/broadcast with ramp/broadcast by comparing both ends.
    bool compare_lt(Expr base_a, Expr stride_a, Expr base_b, Expr stride_b, int width, const LT *op) { //LH
        // Compare two ramps and/or broadcast nodes.
        Expr first_lt = simplify(base_a < base_b);
        Expr last_lt = simplify(base_a + stride_a * (width-1) < base_b + stride_b * (width - 1));
        log(4) << "First " << first_lt << "\n";
        log(4) << "Last  " << last_lt << "\n";
        // If both expressions produce the same result, then that is the result.
        if (equal(first_lt, last_lt)) {
            expr = mutate(new Broadcast(first_lt, width));
            return true; // Tell caller it worked.
        } else {
            if (op) expr = op; // Cannot simplify it. Possible that part of ramp is < and part is not.
        }
        return false;
    }

    bool vector_min(Expr a, Expr base_a, Expr stride_a, Expr b, Expr base_b, Expr stride_b, int width, const Min *op) { //LH
        // Try to find if a <= b; i.e. if (b<a) is false.
        bool a_lt_b = compare_lt(base_a, stride_a, base_b, stride_b, width, 0);
        if (a_lt_b && is_zero(expr)) {
            // Proved that a >= b so minimum is b.
            expr = b;
            return true;
        }
        if (a_lt_b && is_one(expr)) {
            // Proved that a < b so minimum is a.
            expr = a;
            return true;
        }
        bool b_lt_a = compare_lt(base_b, stride_b, base_a, stride_a, width, 0);
        if (b_lt_a && is_zero(expr)) {
            // Proved that a <= b so minimum is a.
            expr = a;
            return true;
        }
        expr = op;
        return false;
    }

    bool vector_max(Expr a, Expr base_a, Expr stride_a, Expr b, Expr base_b, Expr stride_b, int width, const Max *op) { //LH
        // Try to find if a <= b; i.e. if (b<a) is false.
        bool a_lt_b = compare_lt(base_a, stride_a, base_b, stride_b, width, 0);
        if (a_lt_b && is_zero(expr)) {
            // Proved that a >= b so maximum is a.
            expr = a;
            return true;
        }
        if (a_lt_b && is_one(expr)) {
            // Proved that a < b so maximum is b.
            expr = b;
            return true;
        }
        bool b_lt_a = compare_lt(base_b, stride_b, base_a, stride_a, width, 0);
        if (b_lt_a && is_zero(expr)) {
            // Proved that a <= b so maximum is b.
            expr = b;
            return true;
        }
        expr = op;
        return false;
    }

    void visit(const Min *op) {
        Expr a = mutate(op->a), b = mutate(op->b);

        // Move constants to the right to cut down on number of cases to check
        if (is_simple_const(a) && !is_simple_const(b)) {
            std::swap(a, b);
        }

        int ia, ib, ib2, ib3;
        float fa, fb;
        const Ramp *ramp_a = a.as<Ramp>();
        const Ramp *ramp_b = b.as<Ramp>();
        const Broadcast *broadcast_a = a.as<Broadcast>();
        const Broadcast *broadcast_b = b.as<Broadcast>();
        const Add *add_a = a.as<Add>();
        const Add *add_b = b.as<Add>();
        const Min *min_a = a.as<Min>();
        const Min *min_b = b.as<Min>();
        const Min *min_a_a = min_a ? min_a->a.as<Min>() : NULL;
        const Min *min_a_a_a = min_a_a ? min_a_a->a.as<Min>() : NULL;
        const Min *min_a_a_a_a = min_a_a_a ? min_a_a_a->a.as<Min>() : NULL;
		const Max *max_a = a.as<Max>();
		const Min *min_a_max_a = max_a ? max_a->a.as<Min>() : NULL;

        // Sometimes we can do bounds analysis to simplify
        // things. Only worth doing for ints
        
        if (equal(a, b)) {
            expr = a;
        } else if (const_int(a, &ia) && const_int(b, &ib)) {
            expr = std::min(ia, ib);
        } else if (const_float(a, &fa) && const_float(b, &fb)) {
            expr = std::min(fa, fb);
        } else if (const_castint(a, &ia) && const_castint(b, &ib)) {
            if (op->type.is_uint()) {
                expr = make_const(op->type, std::min(((unsigned int) ia), ((unsigned int) ib)));
            } else {
                expr = make_const(op->type, std::min(ia,ib));
            }
        } else if (const_castint(b, &ib) && ib == b.type().imax()) {
            // Compute minimum of expression of type and maximum of type --> expression
            expr = a;
        } else if (const_castint(b, &ib) && ib == b.type().imin()) {
            // Compute minimum of expression of type and minimum of type --> min of type
            expr = b;
        } else if (broadcast_a && broadcast_b) {
            expr = mutate(new Broadcast(new Min(broadcast_a->value, broadcast_b->value), broadcast_a->width));
        } else if (broadcast_a && ramp_b) { //LH
            // Simplify the minimum if possible
            vector_min(a, broadcast_a->value, 0, b, ramp_b->base, ramp_b->stride, ramp_b->width, op);
        } else if (ramp_a && broadcast_b) { //LH
            vector_min(a, ramp_a->base, ramp_a->stride, b, broadcast_b->value, 0, ramp_a->width, op);
        } else if (ramp_a && ramp_b) { //LH
            vector_min(a, ramp_a->base, ramp_a->stride, b, ramp_b->base, ramp_b->stride, ramp_b->width, op);
        } else if (add_a && const_int(add_a->b, &ia) && 
                   add_b && const_int(add_b->b, &ib) && 
                   equal(add_a->a, add_b->a)) {
            // min(x + 3, x - 2) -> x - 2
            if (ia > ib) {
                expr = b;
            } else {
                expr = a;
            }
        } else if (add_a && const_int(add_a->b, &ia) && equal(add_a->a, b)) {
            // min(x + 5, x)
            if (ia > 0) {
                expr = b;
            } else {
                expr = a;
            }
        } else if (add_b && const_int(add_b->b, &ib) && equal(add_b->a, a)) {
            // min(x, x + 5)
            if (ib > 0) {
                expr = a;
            } else {
                expr = b;
            }
        } else if (min_a && is_simple_const(min_a->b) && is_simple_const(b)) {
            // min(min(x, 4), 5) -> min(x, 4)
            expr = new Min(min_a->a, mutate(new Min(min_a->b, b)));
        } else if (global_options.simplify_nested_clamp && 
				   add_a && const_int(add_a->b, &ia) && const_int(b, &ib)) { //LH
			// min(e + k1, k2) -> min(x, k2-k1) + k1   Provided there is no overflow
			// Pushes additions down where they may combine with others.
			// (Subtractions e - k are previously converted to e + -k).
			expr = new Add(new Min(add_a->a, ib - ia), ia);
		} else if (min_a && (equal(min_a->b, b) || equal(min_a->a, b))) {
            // min(min(x, y), y) -> min(x, y)
            expr = a;
        } else if (min_b && (equal(min_b->b, a) || equal(min_b->a, a))) {
            // min(y, min(x, y)) -> min(x, y)
            expr = b;            
        } else if (min_a_a && equal(min_a_a->b, b)) {
            // min(min(min(x, y), z), y) -> min(min(x, y), z)
            expr = a;            
        } else if (min_a_a_a && equal(min_a_a_a->b, b)) {
            // min(min(min(min(x, y), z), w), y) -> min(min(min(x, y), z), w)
            expr = a;            
        } else if (min_a_a_a_a && equal(min_a_a_a_a->b, b)) {
            // min(min(min(min(min(x, y), z), w), l), y) -> min(min(min(min(x, y), z), w), l)
            expr = a;            
        } else if (global_options.simplify_nested_clamp && 
				   max_a && const_int(max_a->b, &ib) && const_int(b, &ib2) && ib2 <= ib) { //LH
			// min(max(x, k1), k2) -> k2 when k1 >= k2
			expr = b;
		} else if (global_options.simplify_nested_clamp && 
				   min_a_max_a && const_int(b, &ib) && const_int(max_a->b, &ib2) && const_int(min_a_max_a->b, &ib3) &&
			ib > ib2 && ib2 < ib3) { //LH
            // min(max(min(x, k1), k2), k3)
			// Expression arises from nested clamp expressions due to inlining
			// k1 <= k2 --> min(k2, k3) to be simplified further.  Max rule should capture that.
			// k2 >= k3 --> k3 according to rule above.
			// Otherwise: min(x, k1) limits upper bound to k1 then max(..., k2) limits lower bound to k2 (where k2 < k1)
			// then min(..., k3) limits upper bound to k3 (where k2 < k3) so overall x is limited to (k2, min(k1,k3))
            expr = new Min(new Max(min_a_max_a->a, ib2), std::min(ib, ib3));            
        } else if (a.same_as(op->a) && b.same_as(op->b)) {
            expr = op;
        } else {
            expr = new Min(a, b);
        }
    }

    void visit(const Max *op) {
        Expr a = mutate(op->a), b = mutate(op->b);

        // Move constants to the right to cut down on number of cases to check
        if (is_simple_const(a) && !is_simple_const(b)) {
            std::swap(a, b);
        }

        int ia, ib, ib2, ib3;
        float fa, fb;
        const Ramp *ramp_a = a.as<Ramp>();
        const Ramp *ramp_b = b.as<Ramp>();
        const Broadcast *broadcast_a = a.as<Broadcast>();
        const Broadcast *broadcast_b = b.as<Broadcast>();
        const Add *add_a = a.as<Add>();
        const Add *add_b = b.as<Add>();
        const Max *max_a = a.as<Max>();
        const Max *max_b = b.as<Max>();
        const Max *max_a_a = max_a ? max_a->a.as<Max>() : NULL;
        const Max *max_a_a_a = max_a_a ? max_a_a->a.as<Max>() : NULL;
        const Max *max_a_a_a_a = max_a_a_a ? max_a_a_a->a.as<Max>() : NULL;
        const Min *min_a = a.as<Min>();
        const Max *max_a_min_a = min_a ? min_a->a.as<Max>() : NULL;

        if (equal(a, b)) {
            expr = a;
        } else if (const_int(a, &ia) && const_int(b, &ib)) {
            expr = std::max(ia, ib);
        } else if (const_float(a, &fa) && const_float(b, &fb)) {
            expr = std::max(fa, fb);
        } else if (const_castint(a, &ia) && const_castint(b, &ib)) {
            if (op->type.is_uint()) {
                expr = make_const(op->type, std::max(((unsigned int) ia), ((unsigned int) ib)));
            } else {
                expr = make_const(op->type, std::max(ia, ib));
            }
        } else if (const_castint(b, &ib) && ib == b.type().imin()) {
            // Compute maximum of expression of type and minimum of type --> expression
            expr = a;
        } else if (const_castint(b, &ib) && ib == b.type().imax()) {
            // Compute maximum of expression of type and maximum of type --> max of type
            expr = b;
        } else if (broadcast_a && broadcast_b) {
            expr = mutate(new Broadcast(new Max(broadcast_a->value, broadcast_b->value), broadcast_a->width));
        } else if (broadcast_a && ramp_b) { //LH
            // Simplify the maximum if possible
            vector_max(a, broadcast_a->value, 0, b, ramp_b->base, ramp_b->stride, ramp_b->width, op);
        } else if (ramp_a && broadcast_b) { //LH
            vector_max(a, ramp_a->base, ramp_a->stride, b, broadcast_b->value, 0, ramp_a->width, op);
        } else if (ramp_a && ramp_b) { //LH
            vector_max(a, ramp_a->base, ramp_a->stride, b, ramp_b->base, ramp_b->stride, ramp_b->width, op);
        } else if (add_a && const_int(add_a->b, &ia) && add_b && const_int(add_b->b, &ib) && equal(add_a->a, add_b->a)) {
            // max(x + 3, x - 2) -> x - 2
            if (ia > ib) {
                expr = a;
            } else {
                expr = b;
            }
        } else if (add_a && const_int(add_a->b, &ia) && equal(add_a->a, b)) {
            // max(x + 5, x)
            if (ia > 0) {
                expr = a;
            } else {
                expr = b;
            }
        } else if (add_b && const_int(add_b->b, &ib) && equal(add_b->a, a)) {
            // max(x, x + 5)
            if (ib > 0) {
                expr = b;
            } else {
                expr = a;
            }
        } else if (max_a && is_simple_const(max_a->b) && is_simple_const(b)) {
            // max(max(x, 4), 5) -> max(x, 5)
            expr = new Max(max_a->a, mutate(new Max(max_a->b, b)));
        } else if (global_options.simplify_nested_clamp && 
				   add_a && const_int(add_a->b, &ia) && const_int(b, &ib)) { //LH
			// max(e + k1, k2) -> max(x, k2-k1) + k1   Provided there is no overflow
			// Pushes additions down where they may combine with others.
			// (Subtractions e - k are previously converted to e + -k).
			expr = new Add(new Max(add_a->a, ib - ia), ia);
        } else if (max_a && (equal(max_a->b, b) || equal(max_a->a, b))) {
            // max(max(x, y), y) -> max(x, y)
            expr = a;
        } else if (max_b && (equal(max_b->b, a) || equal(max_b->a, a))) {
            // max(y, max(x, y)) -> max(x, y)
            expr = b;            
        } else if (max_a_a && equal(max_a_a->b, b)) {
            // max(max(max(x, y), z), y) -> max(max(x, y), z)
            expr = a;            
        } else if (max_a_a_a && equal(max_a_a_a->b, b)) {
            // max(max(max(max(x, y), z), w), y) -> max(max(max(x, y), z), w)
            expr = a;            
        } else if (max_a_a_a_a && equal(max_a_a_a_a->b, b)) {
            // max(max(max(max(max(x, y), z), w), l), y) -> max(max(max(max(x, y), z), w), l)
            expr = a;            
        } else if (global_options.simplify_nested_clamp && 
				   min_a && const_int(min_a->b, &ib) && const_int(b, &ib2) && ib2 >= ib) { //LH
			// max(min(x, k1), k2) -> k2 when k1 <= k2
			expr = b;
		} else if (global_options.simplify_nested_clamp && 
				   max_a_min_a && const_int(b, &ib) && const_int(min_a->b, &ib2) && const_int(max_a_min_a->b, &ib3) &&
			       ib < ib2 && ib2 > ib3) { //LH
            // max(min(max(x, k1), k2), k3)
			// Expression arises from nested clamp expressions due to inlining
			// k1 >= k2 --> max(k2, k3) to be simplified further.  Min rule should capture that.
			// k2 <= k3 --> k3 according to rule above.
			// Otherwise: max(x, k1) limits lower bound to k1 then min(..., k2) limits upper bound to k2 (where k2 > k1)
			// then max(..., k3) limits lower bound to k3 (where k2 > k3) so overall x is limited to (max(k1,k3), k2)
            expr = new Max(new Min(max_a_min_a->a, ib2), std::max(ib, ib3));            
        } else if (a.same_as(op->a) && b.same_as(op->b)) {
            expr = op;
        } else {
            expr = new Max(a, b);
        }
    }

    void visit(const EQ *op) {
        Expr a = mutate(op->a), b = mutate(op->b);
        Expr delta = mutate(a - b);

        const Ramp *ramp_a = a.as<Ramp>();
        const Ramp *ramp_b = b.as<Ramp>();
        const Broadcast *broadcast_a = a.as<Broadcast>();
        const Broadcast *broadcast_b = b.as<Broadcast>();
        const Add *add_a = a.as<Add>();
        const Add *add_b = b.as<Add>();
        const Sub *sub_a = a.as<Sub>();
        const Sub *sub_b = b.as<Sub>();
        const Mul *mul_a = a.as<Mul>();
        const Mul *mul_b = b.as<Mul>();
        
        int ia, ib;

        if (const_castint(a, &ia) && const_castint(b, &ib)) {
            if (a.type().is_uint()) {
                expr = make_bool(((unsigned int) ia) == ((unsigned int) ib), op->type.width);
            } else {
                expr = make_bool(ia == ib, op->type.width);
            }
        } else if (is_zero(delta)) {
            expr = const_true(op->type.width);
        } else if (is_simple_const(delta)) {
            expr = const_false(op->type.width);
        } else if (is_simple_const(a) && !is_simple_const(b)) {
            // Move constants to the right
            expr = mutate(b == a);
        } else if (broadcast_a && broadcast_b) {
            // Push broadcasts outwards
            expr = mutate(new Broadcast(broadcast_a->value == broadcast_b->value, broadcast_a->width));
        } else if (ramp_a && ramp_b && equal(ramp_a->stride, ramp_b->stride)) {
            // Ramps with matching stride
            Expr bases_match = (ramp_a->base == ramp_b->base);
            expr = mutate(new Broadcast(bases_match, ramp_a->width));
        } else if (add_a && add_b && equal(add_a->a, add_b->a)) {
            // Subtract a term from both sides
            expr = mutate(add_a->b == add_b->b);
        } else if (add_a && add_b && equal(add_a->a, add_b->b)) {
            expr = mutate(add_a->b == add_b->a);
        } else if (add_a && add_b && equal(add_a->b, add_b->a)) {
            expr = mutate(add_a->a == add_b->b);
        } else if (add_a && add_b && equal(add_a->b, add_b->b)) {
            expr = mutate(add_a->a == add_b->a);
        } else if (sub_a && sub_b && equal(sub_a->a, sub_b->a)) {
            // Add a term to both sides
            expr = mutate(sub_a->b == sub_b->b);
        } else if (sub_a && sub_b && equal(sub_a->b, sub_b->b)) {
            expr = mutate(sub_a->a == sub_b->a);
        } else if (add_a) {
            // Rearrange so that all adds and subs are on the rhs to cut down on further cases
            expr = mutate(add_a->a == (b - add_a->b));
        } else if (sub_a) {
            expr = mutate(sub_a->a == (b + sub_a->b));
        } else if (add_b && equal(add_b->a, a)) {
            // Subtract a term from both sides
            expr = mutate(make_zero(add_b->b.type()) == add_b->b);
        } else if (add_b && equal(add_b->b, a)) {
            expr = mutate(make_zero(add_b->a.type()) == add_b->a);
        } else if (sub_b && equal(sub_b->a, a)) {
            // Add a term to both sides
            expr = mutate(make_zero(sub_b->b.type()) == sub_b->b);
        } else if (mul_a && mul_b && is_simple_const(mul_a->b) && is_simple_const(mul_b->b) && equal(mul_a->b, mul_b->b)) {
            // Divide both sides by a constant
            assert(!is_zero(mul_a->b) && "Multiplication by zero survived constant folding");
            expr = mutate(mul_a->a == mul_b->a);
        } else if (a.same_as(op->a) && b.same_as(op->b)) {
            expr = op;
        } else {
            expr = new EQ(a, b);
        }
    }

    void visit(const NE *op) {
        expr = mutate(new Not(op->a == op->b));
    }

    void visit(const LT *op) {
        Expr a = mutate(op->a), b = mutate(op->b);
        Expr delta = mutate(a - b);

        const Ramp *ramp_a = a.as<Ramp>();
        const Ramp *ramp_b = b.as<Ramp>();
        const Broadcast *broadcast_a = a.as<Broadcast>();
        const Broadcast *broadcast_b = b.as<Broadcast>();
        const Add *add_a = a.as<Add>();
        const Add *add_b = b.as<Add>();
        const Sub *sub_a = a.as<Sub>();
        const Sub *sub_b = b.as<Sub>();
        const Mul *mul_a = a.as<Mul>();
        const Mul *mul_b = b.as<Mul>();

        int ia, ib;
        
        // Note that the computation of delta could be incorrect if 
        // ia and/or ib are large unsigned integer constants, especially when
        // int is 32 bits on the machine.  
        // Explicit comparison is preferred.
        if (const_castint(a, &ia) && const_castint(b, &ib)) {
            if (a.type().is_uint()) {
                expr = make_bool(((unsigned int) ia) < ((unsigned int) ib), op->type.width);
            } else {
                expr = make_bool(ia < ib, op->type.width);
            }
        } else if (const_castint(a, &ia) && ia == a.type().imax()) {
            // Comparing maximum of type < expression of type.  This can never be true.
            expr = const_false(op->type.width);
        } else if (const_castint(b, &ib) && ib == b.type().imin()) {
            // Comparing expression of type < minimum of type.  This can never be true.
            expr = const_false(op->type.width);
        } else if (is_zero(delta) || is_positive_const(delta)) {
            expr = const_false(op->type.width);
        } else if (is_negative_const(delta)) {
            expr = const_true(op->type.width);
        } else if (broadcast_a && broadcast_b) {
            // Push broadcasts outwards
            expr = mutate(new Broadcast(broadcast_a->value < broadcast_b->value, broadcast_a->width));
        } else if (ramp_a && ramp_b && equal(ramp_a->stride, ramp_b->stride)) {
            // Ramps with matching stride
            Expr bases_lt = (ramp_a->base < ramp_b->base);
            expr = mutate(new Broadcast(bases_lt, ramp_a->width));
        } else if (ramp_a && broadcast_b) { //LH
            compare_lt(ramp_a->base, ramp_a->stride, broadcast_b->value, 0, ramp_a->width, op);
        } else if (broadcast_a && ramp_b) { //LH
            compare_lt(broadcast_a->value, 0, ramp_b->base, ramp_b->stride, ramp_b->width, op);
        } else if (ramp_a && ramp_b) { //LH
            compare_lt(ramp_a->base, ramp_a->stride, ramp_b->base, ramp_b->stride, ramp_b->width, op);
        } else if (is_const(a) && add_b && is_const(add_b->b)) { //LH
            // Constant on LHS and add with constant on RHS
            expr = mutate(a - add_b->b < add_b->a);
        } else if (is_const(a) && sub_b && is_const(sub_b->b)) { //LH
            // Constant on LHS and subtract constant on RHS
            expr = mutate(a + sub_b->b < sub_b->a);
        } else if (is_const(a) && sub_b && is_const(sub_b->a)) { //LH
            // Constant on LHS and subtract from constant on RHS
            expr = mutate(sub_b->b < sub_b->a - a);
        } else if (add_a && add_b && equal(add_a->a, add_b->a)) {
            // Subtract a term from both sides
            expr = mutate(add_a->b < add_b->b);
        } else if (add_a && add_b && equal(add_a->a, add_b->b)) {
            expr = mutate(add_a->b < add_b->a);
        } else if (add_a && add_b && equal(add_a->b, add_b->a)) {
            expr = mutate(add_a->a < add_b->b);
        } else if (add_a && add_b && equal(add_a->b, add_b->b)) {
            expr = mutate(add_a->a < add_b->a);
        } else if (sub_a && sub_b && equal(sub_a->a, sub_b->a)) {
            // Add a term to both sides
            expr = mutate(sub_a->b < sub_b->b);
        } else if (sub_a && sub_b && equal(sub_a->b, sub_b->b)) {
            expr = mutate(sub_a->a < sub_b->a);
        } else if (add_a) {
            // Rearrange so that all adds and subs are on the rhs to cut down on further cases
            expr = mutate(add_a->a < (b - add_a->b));
        } else if (sub_a) {
            expr = mutate(sub_a->a < (b + sub_a->b));
        } else if (add_b && equal(add_b->a, a)) {
            // Subtract a term from both sides
            expr = mutate(make_zero(add_b->b.type()) < add_b->b);
        } else if (add_b && equal(add_b->b, a)) {
            expr = mutate(make_zero(add_b->a.type()) < add_b->a);
        } else if (sub_b && equal(sub_b->a, a)) {
            // Add a term to both sides
            expr = mutate(sub_b->b < make_zero(sub_b->b.type()));
        } else if (mul_a && mul_b && 
                   is_positive_const(mul_a->b) && is_positive_const(mul_b->b) && 
                   equal(mul_a->b, mul_b->b)) {
            // Divide both sides by a constant
            expr = mutate(mul_a->a < mul_b->a);
        } else if (a.same_as(op->a) && b.same_as(op->b)) {
            expr = op;
        } else {
            expr = new LT(a, b);
        }
    }

    void visit(const LE *op) {
        expr = mutate(!(op->b < op->a));
    }

    void visit(const GT *op) {
        expr = mutate(op->b < op->a);
    }

    void visit(const GE *op) {
        expr = mutate(!(op->a < op->b));
    }

    void visit(const And *op) {
        Expr a = mutate(op->a), b = mutate(op->b);

        if (is_one(a)) {
            expr = b;
        } else if (is_one(b)) {
            expr = a;
        } else if (is_zero(a)) {
            expr = a;
        } else if (is_zero(b)) {
            expr = b;
        } else if (a.same_as(op->a) && b.same_as(op->b)) {
            expr = op;
        } else {
            expr = new And(a, b);
        }
    }

    void visit(const Or *op) {
        Expr a = mutate(op->a), b = mutate(op->b);

        if (is_one(a)) {
            expr = a;
        } else if (is_one(b)) {
            expr = b;
        } else if (is_zero(a)) {
            expr = b;
        } else if (is_zero(b)) {
            expr = a;
        } else if (a.same_as(op->a) && b.same_as(op->b)) {
            expr = op;
        } else {
            expr = new Or(a, b);
        }
    }

    void visit(const Not *op) {
        Expr a = mutate(op->a);
        
        if (is_one(a)) {
            expr = make_zero(a.type());
        } else if (is_zero(a)) {
            expr = make_one(a.type());
        } else if (const Not *n = a.as<Not>()) {
            // Double negatives cancel
            expr = n->a;
        } else if (const LE *n = a.as<LE>()) {
            expr = new LT(n->b, n->a);
        } else if (const GE *n = a.as<GE>()) {
            expr = new LT(n->a, n->b);
        } else if (const LT *n = a.as<LT>()) {
            expr = new LE(n->b, n->a);
        } else if (const GT *n = a.as<GT>()) {
            expr = new LE(n->a, n->b);
        } else if (const NE *n = a.as<NE>()) {
            expr = new EQ(n->a, n->b);
        } else if (const EQ *n = a.as<EQ>()) {
            expr = new NE(n->a, n->b);
        } else if (const Broadcast *n = a.as<Broadcast>()) {
            expr = mutate(new Broadcast(!n->value, n->width));
        } else if (a.same_as(op->a)) {
            expr = op;
        } else {
            expr = new Not(a);
        }
    }

    void visit(const Select *op) {
        Expr condition = mutate(op->condition);
        Expr true_value = mutate(op->true_value);
        Expr false_value = mutate(op->false_value);

        if (is_one(condition)) {
            expr = true_value;
        } else if (is_zero(condition)) {
            expr = false_value;
        } else if (equal(true_value, false_value)) {
            expr = true_value;
        } else if (const NE *ne = condition.as<NE>()) {
            // Normalize select(a != b, c, d) to select(a == b, d, c) 
            expr = mutate(new Select(ne->a == ne->b, false_value, true_value));               
        } else if (const LE *le = condition.as<LE>()) {
            // Normalize select(a <= b, c, d) to select(b < a, d, c) 
            expr = mutate(new Select(le->b < le->a, false_value, true_value));               
        } else if (condition.same_as(op->condition) &&
                   true_value.same_as(op->true_value) &&
                   false_value.same_as(op->false_value)) {
            expr = op;
        } else {
            expr = new Select(condition, true_value, false_value);
        }
    }

    void visit(const Load *op) {
        IRMutator::visit(op);
    }

    void visit(const Ramp *op) {
        IRMutator::visit(op);
    }

    void visit(const Broadcast *op) {
        IRMutator::visit(op);
    }

    void visit(const Call *op) {
        IRMutator::visit(op);
    }

# if 0
    // We could be more aggressive about inserting let expressions.
    template<typename T, typename Body> 
    Body simplify_let(const T *op, Scope<Expr> &scope, IRMutator *mutator) {
        // If the value is trivial, make a note of it in the scope so
        // we can subs it in later
        Expr value = mutator->mutate(op->value);
        Body body = op->body;
        assert(value.defined());
        assert(body.defined());
        const Ramp *ramp = value.as<Ramp>();
        const Broadcast *broadcast = value.as<Broadcast>();        
        const Variable *var = value.as<Variable>();
        string wrapper_name;
        Expr wrapper_value;
        if (is_simple_const(value)) {
            // Substitute the value wherever we see it
            scope.push(op->name, value);
        } else if (ramp && is_simple_const(ramp->stride)) {
            wrapper_name = op->name + ".base" + unique_name('.');

            // Make a new name to refer to the base instead, and push the ramp inside
            Expr val = new Variable(ramp->base.type(), wrapper_name);
            Expr base = ramp->base;

            // If it's a multiply, move the multiply part inwards
            const Mul *mul = base.as<Mul>();
            const IntImm *mul_b = mul ? mul->b.as<IntImm>() : NULL;
            if (mul_b) {
                base = mul->a;
                val = new Ramp(val * mul->b, ramp->stride, ramp->width);
            } else {
                val = new Ramp(val, ramp->stride, ramp->width);
            }

            scope.push(op->name, val);

            wrapper_value = base;
        } else if (broadcast) {
            wrapper_name = op->name + ".value" + unique_name('.');

            // Make a new name refer to the scalar version, and push the broadcast inside            
            scope.push(op->name, 
                       new Broadcast(new Variable(broadcast->value.type(), 
                                                  wrapper_name), 
                                     broadcast->width));
            wrapper_value = broadcast->value;
        } else if (var) {
            // This var is just equal to another var. We should subs
            // it in only if the second var is still in scope at the
            // usage site (this is checked in the visit(Variable*) method.
            scope.push(op->name, var);
        } else {
            // Push a empty expr on, to make sure we hide anything
            // else with the same name until this goes out of scope
            scope.push(op->name, Expr());
        }

        // Before we enter the body, track the alignment info 
        bool wrapper_tracked = false;
        if (wrapper_value.defined() && wrapper_value.type() == Int(32)) {
            ModulusRemainder mod_rem = modulus_remainder(wrapper_value, alignment_info);
            alignment_info.push(wrapper_name, mod_rem);
            wrapper_tracked = true;
        }

        bool value_tracked = false;
        if (value.type() == Int(32)) {
            ModulusRemainder mod_rem = modulus_remainder(value, alignment_info);
            alignment_info.push(op->name, mod_rem);
            value_tracked = true;
        }

        body = mutator->mutate(body);

        if (value_tracked) {
            alignment_info.pop(op->name);
        }
        if (wrapper_tracked) {
            alignment_info.pop(wrapper_name);
        }

        scope.pop(op->name);

        if (wrapper_value.defined()) {
            return new T(wrapper_name, wrapper_value, new T(op->name, value, body));
        } else if (body.same_as(op->body) && value.same_as(op->value)) {
            return op;
        } else {
            return new T(op->name, value, body);
        }        
    }


    void visit(const Let *op) {
        expr = simplify_let<Let, Expr>(op, scope, this);
    }

    void visit(const LetStmt *op) {
        stmt = simplify_let<LetStmt, Stmt>(op, scope, this);
    }
#endif

    // Operations that simply defer to the parent
    // void visit(const PrintStmt *op) {
    // void visit(const AssertStmt *op) {
    // void visit(const Pipeline *op) {
    // void visit(const For *op) {
    // void visit(const Store *op) {
    // void visit(const Provide *op) {
    // void visit(const Allocate *op) {
    // void visit(const Realize *op) {
    // void visit(const Block *op) {        
#endif    
};

// The base solver is designed to compute forward intervals but does not
// understand particular interpretations of node.  It returns a single Solution,
// since it does not recognise nodes that would require a new Solution.
void solver(Expr e, std::vector<Interval> intervals, std::vector<std::string> varlist) {
    Solver s;
    e.accept(&s);
    
}

namespace {
// Method to check the operation of the Solver class.
// This is the core class, simplifying expressions that include Target and Solve.
void checkSolver(Expr a, Expr b) {
    Solver s;
    s.targets.push_back("x");
    s.targets.push_back("y");
    std::cout << "checkSolver: " << a << "\n";
    std::cout << "  expect: " << b << "\n";
    Expr r = s.mutate(a);
    std::cout << "  result: " << r << "\n";
    if (!equal(b, r)) {
        std::cout << std::endl << "Simplification failure: " << std::endl;
        std::cout << "Input: " << a << std::endl;
        std::cout << "Output: " << r << std::endl;
        std::cout << "Expected output: " << b << std::endl;
        assert(false);
    }
}
}

void solver_test() {
    Var x("x"), y("y"), c("c"), d("d");
    
    checkSolver(solve(x, Interval(0,10)), solve(x, Interval(0,10)));
    checkSolver(solve(x + 4, Interval(0,10)), solve(x, Interval(-4,6)) + 4);
    checkSolver(solve(4 + x, Interval(0,10)), solve(x, Interval(-4,6)) + 4);
    checkSolver(solve(x + 4 + d, Interval(0,10)), solve(x, Interval(-4-d, 6-d)) + d + 4);
    checkSolver(solve(x - d, Interval(0,10)), solve(x, Interval(d, d+10)) - d);
    checkSolver(solve(x - (4 - d), Interval(0,10)), solve(x, Interval(4-d, 14-d)) + d + -4);
    checkSolver(solve(x - 4 - d, Interval(0,10)), solve(x, Interval(d+4, d+14)) - d + -4);
    // Solve 4-x on the interval (0,10).
    // 0 <= 4-x <= 10.
    // -4 <= -x <= 6.  solve(-x) + 4
    // 4 >= x >= -6.   -solve(x) + 4  i.e.  4 - solve(x)
    checkSolver(solve(4 - x, Interval(0,10)), 4 - solve(x, Interval(-6,4)));
    checkSolver(solve(4 - d - x, Interval(0,10)), 4 - d - solve(x, Interval(-6 - d, 4 - d)));
    checkSolver(solve(4 - d - x, Interval(0,10)) + 1, 5 - d - solve(x, Interval(-6 - d, 4 - d)));
    // Solve c - (x + d) on (0,10).
    // 0 <= c - (x + d) <= 10.
    // -c <= -(x+d) <= 10-c.
    // c >= x+d >= c-10.
    // c-d >= d >= c-d-10.
    checkSolver(solve(c - (x + d), Interval(0,10)), c - d - solve(x, Interval(c-d+-10, c-d)));
    
    checkSolver(solve(x * 2, Interval(0,10)), solve(x, Interval(0,5)) * 2);
    checkSolver(solve(x * 3, Interval(1,17)), solve(x, Interval(1,5)) * 3);
    checkSolver(solve(x * -3, Interval(1,17)), solve(x, Interval(-5,-1)) * -3);
    checkSolver(solve((x + 3) * 2, Interval(0,10)), solve(x, Interval(-3, 2)) * 2 + 6);
    // Solve 0 <= (x + 4) * 3 <= 10
    // 0 <= (x + 4) <= 3
    // -4 <= x <= -1
    checkSolver(solve((x + 4) * 3, Interval(0,10)), solve(x, Interval(-4, -1)) * 3 + 12);
    // Solve 0 <= (x + c) * -3 <= 10
    // 0 >= (x + c) >= -3
    // -c >= x >= -3 - c
    checkSolver(solve((x + c) * -3, Interval(0,10)), (solve(x, Interval(-3 - c, 0 - c)) + c) * -3);
    
    checkSolver(solve(x / 3, Interval(0,10)), solve(x, Interval(0, 32)) / 3);
    checkSolver(solve(x / -3, Interval(0,10)), solve(x, Interval(-30,2)) / -3);
    // Solve 1 <= (x + c) / 3 <= 17
    // 3 <= (x + c) <= 53
    // 3 - c <= x <= 53 - c
    checkSolver(solve((x + c) / 3, Interval(1,17)), (solve(x, Interval(3 - c, 53 - c)) + c) / 3);
    
    checkSolver(solve(x + 4, Interval(0,new Infinity(+1))), solve(x, Interval(-4,new Infinity(+1))) + 4);
    checkSolver(solve(x + 4, Interval(new Infinity(-1),10)), solve(x, Interval(new Infinity(-1),6)) + 4);
    
    std::cout << "Solve test passed" << std::endl;
}

// end namespace Internal
}
}
