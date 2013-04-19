#include "Simplify.h"
#include "IROperator.h"
#include "IREquality.h"
#include "IRPrinter.h"
#include "IRMutator.h"
#include "Scope.h"
#include "Var.h"
#include "Log.h"
#include "ModulusRemainder.h"
#include <iostream>

namespace Halide { 
namespace Internal {

using std::string;

bool is_simple_const(Expr e) {
    return is_const(e) && (!e.as<Cast>());
}

// Is a constant representable as a certain type
int do_indirect_int_cast(Type t, int x) {
    if (t == UInt(1)) {
        return x ? 1 : 0;
    } else if (t.is_int() || t.is_uint()) {
        return int_cast_constant(t, x);
    } else if (t == Float(32)) {
        return (int)((float)x);
    } else if (t == Float(64)) {
        return (int)((double)x);
    } else {
        assert(false && "Can't do an indirect int cast via this type");
        return 0;
    }
}

// Implementation of Halide div and mod operators

class Simplify : public IRMutator {
protected:
    Scope<Expr> scope;

    Scope<ModulusRemainder> alignment_info;

    using IRMutator::visit;

    virtual void visit(const IntImm *op) {
        IRMutator::visit(op);
    }

    virtual void visit(const FloatImm *op) {
        IRMutator::visit(op);
    }

    bool const_float(Expr e, float *f) {
        const FloatImm *c = e.as<FloatImm>();
        if (c) {
            *f = c->value;
            return true;
        } else {
            return false;
        }
    }

    bool const_int(Expr e, int *i) {
        const IntImm *c = e.as<IntImm>();
        if (c) {
            *i = c->value;
            return true;
        } else {
            return false;
        }
    }


    /* Recognise an integer or cast integer and fetch its value.
     * Only matches if the number of bits of the cast integer does not exceed
     * the number of bits of an int in the compiler, because simplification
     * uses type int for its calculations. */
    bool const_castint(Expr e, int *i) {
        const IntImm *intimm = e.as<IntImm>();
        const Cast *cast = e.as<Cast>();
        if (intimm) {
            *i = intimm->value;
            return true;
        } else if (cast && (cast->type.is_int() || cast->type.is_uint()) && 
                   cast->type.bits <= (int) (sizeof(int) * 8)) {
            const IntImm *imm = cast->value.as<IntImm>();
            if (imm) {
                // When fetching a cast integer, ensure that the return
                // value is in the correct range (i.e. the canonical value) for the cast type.
                *i = int_cast_constant(cast->type, imm->value);
                return true;
            } else {
                return false;
            }
        } else {
            return false;
        }
    }
    
    /* Recognise Infinity node on one or both sides of a binary operator and
     * return a code. Codes are #define constants where 
     * F represents a finite value, P represents positive infinity and N represents
     * negative infinity. */
#define NN 1
#define NF 2
#define NP 4
#define FN 8
#define FF 16
#define FP 32
#define PN 64
#define PF 128
#define PP 256
    int infinity_code(Expr a, Expr b) {
        int count_a = infinity_count(a);
        int count_b = infinity_count(b);
        int bit = 0;
        if (count_a > 0) bit += 6; // PN, PF, PP
        else if (count_a == 0) bit += 3; // FN, FF, FP
        if (count_b > 0) bit += 2; // xP
        else if (count_b == 0) bit++; // xF
        return (1 << bit);
    }

    virtual void visit(const Cast *op) {
        Expr value = mutate(op->value);        
        const Cast *cast = value.as<Cast>();
        float f;
        int i;
        if (value.type() == op->type) {
            expr = value;
        } else if (op->type == Int(32) && const_float(value, &f)) {
            expr = new IntImm((int)f);
        } else if (op->type == Float(32) && const_int(value, &i)) {
            expr = new FloatImm((float)i);
        } else if (op->type == Int(32) && cast && const_int(cast->value, &i)) {
            // Cast to something then back to int
            expr = do_indirect_int_cast(cast->type, i);
        } else if (!op->type.is_float() && 
                   op->type.bits <= 32 && 
                   const_int(value, &i) && 
                   do_indirect_int_cast(op->type, i) != i) {
            // Rewrite things like cast(UInt(8), 256) to cast(UInt(8),
            // 0), so any later peephole matching that ignores casts
            // doesn't get confused.
            expr = new Cast(op->type, do_indirect_int_cast(op->type, i));
        } else if (value.same_as(op->value)) {
            expr = op;
        } else {
            expr = new Cast(op->type, value);
        }
    }

    virtual void visit(const Variable *op) {
        if (scope.contains(op->name)) {
            Expr replacement = scope.get(op->name);

            //std::cout << "Pondering replacing " << op->name << " with " << replacement << std::endl;

            // if expr is defined, we should substitute it in (unless
            // it's a var that has been hidden by a nested scope).
            if (replacement.defined()) {
                //std::cout << "Replacing " << op->name << " of type " << op->type << " with " << replacement << std::endl;
                assert(replacement.type() == op->type);
                // If it's a naked var, and the var it refers to
                // hasn't gone out of scope, just replace it with that
                // var
                if (const Variable *v = replacement.as<Variable>()) {
                    if (scope.contains(v->name)) {
                        if (scope.depth(v->name) < scope.depth(op->name)) {
                            expr = replacement;
                        } else {
                            // Uh oh, the variable we were going to
                            // subs in has been hidden by another
                            // variable of the same name, better not
                            // do anything.
                            expr = op;
                        }
                    } else {
                        // It is a variable, but the variable this
                        // refers to hasn't been encountered. It must
                        // be a uniform, so it's safe to substitute it
                        // in.
                        expr = replacement;
                    }
                } else {
                    // It's not a variable, and a replacement is defined
                    expr = replacement;
                }
            } else {
                // This expression was not something deemed
                // substitutable - no replacement is defined.
                expr = op;
            }
        } else {
            // We never encountered a let that defines this var. Must
            // be a uniform. Don't touch it.
            expr = op;
        }
    }

    virtual void visit(const Add *op) {
        log(3) << depth << " Add simplify " << Expr(op) << "\n";

        int ia, ib;
        float fa, fb;

        Expr a = mutate(op->a), b = mutate(op->b);

        // rearrange const + varying to varying + const, to cut down
        // on cases to check
        if (is_simple_const(a) && !is_simple_const(b)) std::swap(a, b);

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
        
        // Check for infinity cases
        int inf = infinity_code(a, b);
        if (inf & (PP | PF | NN | NF)) {
            expr = a; // Left infinity dominates
        } else if (inf & (FN | FP)) {
            expr = b; // Right infinity dominates
        } else if (inf & (PN | NP)) {
            assert(0 && "Conflicting infinity"); // Addition of conflicting infinities
        } else if (const_int(a, &ia) &&
            const_int(b, &ib)) {
            expr = ia + ib;
            // const int + const int
        } else if (const_float(a, &fa) &&
                   const_float(b, &fb)) {
            // const float + const float
            expr = fa + fb;
        } else if (is_zero(b)) {
            expr = a;
        } else if (is_zero(a)) {
            expr = b;
        } else if (const_castint(a, &ia) && const_castint(b, &ib)) {
            if (op->type.is_uint()) {
                expr = make_const(op->type, ((unsigned int) ia) + ((unsigned int) ib));
            } else {
                expr = make_const(op->type, ia + ib);
            }
        } else if (ramp_a && ramp_b) {
            // Ramp + Ramp
            expr = mutate(new Ramp(ramp_a->base + ramp_b->base,
                                   ramp_a->stride + ramp_b->stride, ramp_a->width));
        } else if (ramp_a && broadcast_b) {
            // Ramp + Broadcast
            expr = mutate(new Ramp(ramp_a->base + broadcast_b->value, 
                                   ramp_a->stride, ramp_a->width));
        } else if (broadcast_a && ramp_b) {
            // Broadcast + Ramp
            expr = mutate(new Ramp(broadcast_a->value + ramp_b->base, 
                                   ramp_b->stride, ramp_b->width));
        } else if (broadcast_a && broadcast_b) {
            // Broadcast + Broadcast
            expr = new Broadcast(mutate(broadcast_a->value + broadcast_b->value),
                                 broadcast_a->width);
        } else if (equal(a, b)) { //LH
            // Adding an expression to itself - multiply by 2 instead
            // (which ends up being a bit shift operation for efficiency)
            expr = mutate(a * 2);
        } else if (add_a && is_simple_const(add_a->b)) {
            // In ternary expressions, pull constants outside
            if (is_simple_const(b)) expr = mutate(add_a->a + (add_a->b + b));
            else expr = mutate((add_a->a + b) + add_a->b);
        } else if (add_b && is_simple_const(add_b->b)) {
            expr = mutate((a + add_b->a) + add_b->b);
        } else if (sub_a && is_simple_const(sub_a->a)) { //LH
            // (kaa - ab) + kb --> (kaa + kb) - ab
            // (kaa - ab) + b --> (kaa + b) - ab
            expr = mutate((sub_a->a + b) - sub_a->b);
        } else if (sub_a && equal(b, sub_a->b)) {
            // Additions that cancel an inner term
            expr = sub_a->a;
        } else if (sub_b && equal(a, sub_b->b)) {            
            expr = sub_b->a;
        } else if (mul_a && mul_b && equal(mul_a->a, mul_b->a)) {
            // Pull out common factors a*x + b*x
            expr = mutate(mul_a->a * (mul_a->b + mul_b->b));
        } else if (mul_a && mul_b && equal(mul_a->b, mul_b->a)) {
            expr = mutate(mul_a->b * (mul_a->a + mul_b->b));
        } else if (mul_a && mul_b && equal(mul_a->b, mul_b->b)) {
            expr = mutate(mul_a->b * (mul_a->a + mul_b->a));
        } else if (mul_a && mul_b && equal(mul_a->a, mul_b->b)) {
            expr = mutate(mul_a->a * (mul_a->b + mul_b->a));
        } else if (mul_a && equal(mul_a->a, b) && ! is_const(b)) { //LH
            expr = mutate(b * (mul_a->b + 1));
        } else if (mul_a && equal(mul_a->b, b) && ! is_const(b)) { //LH
            expr = mutate(b * (mul_a->a + 1));
        } else if (mul_b && equal(mul_b->a, a) && ! is_const(a)) { //LH
            expr = mutate(a * (mul_b->b + 1));
        } else if (mul_b && equal(mul_b->b, a) && ! is_const(a)) { //LH
            expr = mutate(a * (mul_b->a + 1));
        } else if ((b.as<Max>() || b.as<Min>()) && ! (a.as<Max>() || a.as<Min>())) { //LH
            // Push max/min to LHS of add to reduce cases elsewhere (especially in LT).
            expr = mutate(b + a);
        } else if (a.same_as(op->a) && b.same_as(op->b)) {
            // If we've made no changes, and can't find a rule to apply, return the operator unchanged.
            expr = op;
        } else {
            expr = new Add(a, b);
        }
        log(3) << depth << " Add simplified to " << expr << "\n";
    }

    virtual void visit(const Sub *op) {
        log(3) << depth << " Sub simplify " << Expr(op) << "\n";

        Expr a = mutate(op->a), b = mutate(op->b);

        int ia, ib; 
        float fa, fb;

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

        // Check for infinity cases
        int inf = infinity_code(a, b);
        if (inf & (PN | PF | NP | NF)) {
            expr = a; // Left infinity dominates
        } else if (inf & (FN | FP)) {
            // Right infinity dominates but has to be negated
            const Infinity *inf_b = b.as<Infinity>();
            expr = new Infinity(-inf_b->count);
        } else if (inf & (PP | NN)) {
            assert(0 && "Conflicting infinity"); // subtraction of conflicting infinities
        } else if (is_zero(b)) {
            expr = a;
        } else if (equal(a, b)) {
            expr = make_zero(op->type);
        } else if (const_int(a, &ia) && const_int(b, &ib)) {
            expr = ia - ib;
        } else if (const_float(a, &fa) && const_float(b, &fb)) {
            expr = fa - fb;
        } else if (const_int(b, &ib)) {
            expr = mutate(a + (-ib));
        } else if (const_float(b, &fb)) {
            expr = mutate(a + (-fb));
        } else if (const_castint(a, &ia) && const_castint(b, &ib)) {
            if (op->type.is_uint()) {
                expr = make_const(op->type, ((unsigned int) ia) - ((unsigned int) ib));
            } else {
                expr = make_const(op->type, ia - ib);
            }
        } else if (ramp_a && ramp_b) {
            // Ramp - Ramp
            expr = mutate(new Ramp(ramp_a->base - ramp_b->base,
                                   ramp_a->stride - ramp_b->stride, ramp_a->width));
        } else if (ramp_a && broadcast_b) {
            // Ramp - Broadcast
            expr = mutate(new Ramp(ramp_a->base - broadcast_b->value, 
                                   ramp_a->stride, ramp_a->width));
        } else if (broadcast_a && ramp_b) {
            // Broadcast - Ramp
            expr = mutate(new Ramp(broadcast_a->value - ramp_b->base, 
                                   make_zero(ramp_b->stride.type())- ramp_b->stride,
                                   ramp_b->width));
        } else if (broadcast_a && broadcast_b) {
            // Broadcast + Broadcast
            expr = new Broadcast(mutate(broadcast_a->value - broadcast_b->value),
                                 broadcast_a->width);
        } else if (add_a && equal(add_a->b, b)) {
            // Ternary expressions where a term cancels
            expr = add_a->a;
        } else if (add_a && equal(add_a->a, b)) {
            expr = add_a->b;
        } else if (add_b && equal(add_b->b, a)) {
            expr = mutate(make_zero(add_b->a.type()) - add_b->a);
        } else if (add_b && equal(add_b->a, a)) {
            expr = mutate(make_zero(add_b->a.type()) - add_b->b);
        } else if (add_a && is_simple_const(add_a->b)) {
            // In ternary expressions, pull constants outside
            if (is_simple_const(b)) expr = mutate(add_a->a + (add_a->b - b));
            else expr = mutate((add_a->a - b) + add_a->b);
        } else if (add_b && is_simple_const(add_b->b)) {
            // ka - (ba + kbb) --> (ka - kbb) - ba
            if (is_simple_const(a)) expr = mutate((a - add_b->b) - add_b->a); //LH
            // a - (ba + kbb) --> (a - ba) - kbb
            else expr = mutate((a - add_b->a) - add_b->b);
        } else if (sub_a && is_simple_const(sub_a->a) && is_simple_const(b)) {
            expr = mutate((sub_a->a - b) - sub_a->b);
        } else if (sub_b && is_simple_const(sub_b->b)) {
            // ka - (ba - kbb) --> (ka + kbb) - ba
            if (is_simple_const(a)) expr = mutate((a + sub_b->b) - sub_b->a);
            // a - (ba - kbb) --> (a - ba) + kbb
            else expr = mutate((a - sub_b->a) + sub_b->b); //LH
        } else if (sub_b && is_simple_const(sub_b->a)) { //LH
            // ka - (kba - bb) --> bb + (ka - kba)
            if (is_simple_const(a)) expr = mutate(sub_b->b + (a - sub_b->a));
            // a - (kba - bb) --> (a + bb) - kba
            else expr = mutate((a + sub_b->b) - sub_b->a);
        } else if (mul_a && mul_b && equal(mul_a->a, mul_b->a)) {
            // Pull out common factors a*x + b*x
            expr = mutate(mul_a->a * (mul_a->b - mul_b->b));
        } else if (mul_a && mul_b && equal(mul_a->b, mul_b->a)) {
            expr = mutate(mul_a->b * (mul_a->a - mul_b->b));
        } else if (mul_a && mul_b && equal(mul_a->b, mul_b->b)) {
            expr = mutate(mul_a->b * (mul_a->a - mul_b->a));
        } else if (mul_a && mul_b && equal(mul_a->a, mul_b->b)) {
            expr = mutate(mul_a->a * (mul_a->b - mul_b->a));
        } else if (mul_a && equal(mul_a->a, b) && ! is_const(b)) { //LH
            expr = mutate(b * (mul_a->b - 1));
        } else if (mul_a && equal(mul_a->b, b) && ! is_const(b)) { //LH
            expr = mutate(b * (mul_a->a - 1));
        } else if (mul_b && equal(mul_b->a, a) && ! is_const(a)) { //LH
            expr = mutate(a * (1 - mul_b->b));
        } else if (mul_b && equal(mul_b->b, a) && ! is_const(a)) { //LH
            expr = mutate(a * (1 - mul_b->a));
        } else if (a.same_as(op->a) && b.same_as(op->b)) {
            expr = op;
        } else {
            expr = new Sub(a, b);
        }
        log(3) << depth << " Sub simplified to " << expr << "\n";
    }

    virtual void visit(const Mul *op) {
        Expr a = mutate(op->a), b = mutate(op->b);

        if (is_simple_const(a)) std::swap(a, b);

        int ia, ib; 
        float fa, fb;

        const Ramp *ramp_a = a.as<Ramp>();
        const Ramp *ramp_b = b.as<Ramp>();
        const Broadcast *broadcast_a = a.as<Broadcast>();
        const Broadcast *broadcast_b = b.as<Broadcast>();
        const Add *add_a = a.as<Add>();
        const Mul *mul_a = a.as<Mul>();

        // Check for infinity cases
        int inf = infinity_code(a, b);
        if (inf & (PP | NP)) {
            expr = a; // Left infinity dominates
        } else if (inf & (PN)) {
            // Right infinity dominates
            expr = b;
        } else if ((inf & (PF | NF)) && is_positive_const(b)) {
            // infinity multiplied by positive finite.
            expr = a;
        } else if ((inf & (PF | NF)) && is_negative_const(b)) {
            // infinity multiplied by negative finite.
            const Infinity *inf_a = a.as<Infinity>();
            expr = new Infinity(-inf_a->count);
        } else if (is_zero(b)) {
            expr = b;
        } else if (is_one(b)) {
            expr = a;
        } else if (const_int(a, &ia) && const_int(b, &ib)) {
            expr = ia*ib;
        } else if (const_float(a, &fa) && const_float(b, &fb)) {
            expr = fa*fb;
        } else if (const_castint(a, &ia) && const_castint(b, &ib)) {
            if (op->type.is_uint()) {
                expr = make_const(op->type, ((unsigned int) ia) * ((unsigned int) ib));
            } else {
                expr = make_const(op->type, ia * ib);
            }
        } else if (broadcast_a && broadcast_b) {
            expr = new Broadcast(mutate(broadcast_a->value * broadcast_b->value), broadcast_a->width);
        } else if (ramp_a && broadcast_b) {
            Expr m = broadcast_b->value;
            expr = mutate(new Ramp(ramp_a->base * m, ramp_a->stride * m, ramp_a->width));
        } else if (broadcast_a && ramp_b) {
            Expr m = broadcast_a->value;
            expr = mutate(new Ramp(m * ramp_b->base, m * ramp_b->stride, ramp_b->width));
        } else if (add_a && is_simple_const(add_a->b) && is_simple_const(b)) {
            expr = mutate(add_a->a * b + add_a->b * b);
        } else if (mul_a && is_simple_const(mul_a->b) && is_simple_const(b)) {
            expr = mutate(mul_a->a * (mul_a->b * b));
        } else if (a.same_as(op->a) && b.same_as(op->b)) {
            expr = op;
        } else {
            expr = new Mul(a, b);
        }
    }

    virtual void visit(const Div *op) {
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

        // Check for infinity cases
        int inf = infinity_code(a, b);
        if (inf & (PP | NP | PN | NN)) {
            assert (0 && "Conflicting infinity in division");
        } else if (inf & (FP | FN)) {
            // Division by infinity yields zero.
            expr = make_zero(a.type());
        } else if ((inf & (PF | NF)) && is_positive_const(b)) {
            // infinity divided by positive finite.
            expr = a;
        } else if ((inf & (PF | NF)) && is_negative_const(b)) {
            // infinity divided by negative finite: negate the infinity.
            const Infinity *inf_a = a.as<Infinity>();
            expr = new Infinity(-inf_a->count);
        } else if (is_zero(a)) {
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

    virtual void visit(const Mod *op) {
        Expr a = mutate(op->a), b = mutate(op->b);

        // Check for infinity cases
        int inf = infinity_code(a, b);
        if ((inf & FP) && (is_positive_const(a) || is_zero(a))) {
            // a mod positive infinity returns a if a >= 0.
            expr = a;
            return;
        } else if ((inf & FN) && (is_negative_const(a) || is_zero(a))) {
            // a mod negative infinity returns a if a <= 0.
            expr = a;
            return;
        } else if (inf & (PP | NN)) {
            // Infinity mod infinity returns infinity
            expr = a;
            return;
        } else if (inf & (NP | PN | NF | PF)) {
            // infinity mod finite or mod opposite infinity is error
            assert(0 && "Infinity conflict in modulus");
            return;
        }
        
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

    virtual void visit(const Min *op) {
        Expr a = mutate(op->a), b = mutate(op->b);

        // Move constants to the right to cut down on number of cases to check
        if (is_simple_const(a) && !is_simple_const(b)) {
            std::swap(a, b);
        }

        // Check for infinity cases
        int inf = infinity_code(a, b);
        if (inf & (PP | FP | NP | NF | NN)) {
            // The first parameter is the minimum
            expr = a;
            return;
        } else if (inf & (PF | PN | FN)) {
            // The second parameter is the minimum
            expr = b;
            return;
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

    virtual void visit(const Max *op) {
        Expr a = mutate(op->a), b = mutate(op->b);

        // Check for infinity cases
        int inf = infinity_code(a, b);
        if (inf & (PP | FP | NP | NF | NN)) {
            // The second parameter is the maximum
            expr = b;
            return;
        } else if (inf & (PF | PN | FN)) {
            // The first parameter is the maximum
            expr = a;
            return;
        }
        
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

    virtual void visit(const EQ *op) {
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

    virtual void visit(const NE *op) {
        expr = mutate(new Not(op->a == op->b));
    }

    virtual void visit(const LT *op) {
        Expr a = mutate(op->a), b = mutate(op->b);

        // Check for infinity cases
        int inf = infinity_code(a, b);
        if (inf & (FP | NP | NF)) {
            // f < P; N < P; N < f: return true
            expr = const_true(op->type.width);
            return;
        } else if (inf & (PF | PN | FN)) {
            // P < f; P < N; f < N: return false
            expr = const_false(op->type.width);
            return;
        } else if (inf & (PP | NN)) {
            assert(0 && "Infinity conflict in LT");
            return;
        }

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
        const Min *min_a = a.as<Min>();
        const Min *min_b = b.as<Min>();
        const Max *max_a = a.as<Max>();
        const Max *max_b = b.as<Max>();

        int ia, ib;
        bool disproved;
        
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
        } else if (add_b && !(min_a || max_a) && (add_b->a.as<Min>() || add_b->a.as<Max>())) {
            // Push the add to the other side to expose the min/max
            expr = mutate(a - add_b->b < add_b->a);
        } else if (sub_b && !(min_a || max_a) && (sub_b->a.as<Min>() || sub_b->a.as<Max>())) {
            // Push the subtract to the other side to expose the min/max
            expr = mutate(a + sub_b->b < sub_b->a);
        } else if (sub_b && !(min_a || max_a) && (sub_b->b.as<Min>() || sub_b->b.as<Max>())) {
            // Push the min/max to the other side to expose it
            expr = mutate(sub_b->b < sub_b->a - a);
        } else if (add_a && ! min_b && ! max_b) {
            // Rearrange so that all adds and subs are on the rhs to cut down on further cases
            // Exception: min/max on RHS keeps the add/sub away
            expr = mutate(add_a->a < (b - add_a->b));
        } else if (sub_a && ! min_b && ! max_b) {
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
        } else if (min_a && (proved(min_a->b < b, disproved) || proved(min_a->a < b))) { //LH
            // Prove min(x,y) < b by proving x < b or y < b
            // Because constants are pushed to RHS, it is more likely that min_a->b < b
            // can be resolved quickly
            expr = const_true();
        } else if (min_a && (disproved /* proved(min_a->b >= b) */ && proved(min_a->a >= b))) { //LH
            // Prove min(x,y) >= b by proving x >= b and y >= b
            expr = const_false();
        } else if (min_b && (proved(a >= min_b->b, disproved) || proved(a >= min_b->a))) { //LH
            // Prove a >= min(x,y) by proving a >= x or a >= y
            expr = const_false();
        } else if (min_b && (disproved /* proved(a < min_b->b) */ && proved(a < min_b->a))) { //LH
            // Prove a < min(x,y) by proving a < x and a < y
            expr = const_true();
        } else if (max_a && (proved(max_a->b >= b, disproved) || proved(max_a->a >= b))) { //LH
            // Prove that max(x,y) >= b by proving x >= b or y >= b
            expr = const_false();
        } else if (max_a && (disproved /* proved(max_a->b < b) */ && proved(max_a->a < b))) { //LH
            // Prove that max(x,y) < b by proving x < b and y < b
            expr = const_true();
        } else if (max_b && (proved(a < max_b->b, disproved) || proved(a < max_b->b))) { //LH
            // Prove that a < max(x,y) by proving a < x or a < y
            expr = const_true();
        } else if (max_b && (disproved /* proved(a >= max_b->b) */ && proved(a >= max_b->a))) { //LH
            // Prove that a >= max(x,y) by proving a >= x and a >= y
            expr = const_false();
        } else if (a.same_as(op->a) && b.same_as(op->b)) {
            expr = op;
        } else {
            expr = new LT(a, b);
        }
    }

    virtual void visit(const LE *op) {
        expr = mutate(!(op->b < op->a));
    }

    virtual void visit(const GT *op) {
        expr = mutate(op->b < op->a);
    }

    virtual void visit(const GE *op) {
        expr = mutate(!(op->a < op->b));
    }

    virtual void visit(const And *op) {
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

    virtual void visit(const Or *op) {
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

    virtual void visit(const Not *op) {
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

    virtual void visit(const Select *op) {
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

    virtual void visit(const Load *op) {
        IRMutator::visit(op);
    }

    virtual void visit(const Ramp *op) {
        IRMutator::visit(op);
    }

    virtual void visit(const Broadcast *op) {
        IRMutator::visit(op);
    }

    virtual void visit(const Call *op) {
        IRMutator::visit(op);
    }

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


    virtual void visit(const Let *op) {
        expr = simplify_let<Let, Expr>(op, scope, this);
    }

    virtual void visit(const LetStmt *op) {
        stmt = simplify_let<LetStmt, Stmt>(op, scope, this);
    }

    virtual void visit(const PrintStmt *op) {
        IRMutator::visit(op);
    }

    virtual void visit(const AssertStmt *op) {
        IRMutator::visit(op);
    }

    virtual void visit(const Pipeline *op) {
        IRMutator::visit(op);
    }

    virtual void visit(const For *op) {
        if (global_options.lift_let) {
            // If there is a Let immediately inside the For, lift it out unless
            // it redefines the For variable name.  This change is made before processing
            // the For node and the enclosed LetStmt.
            const LetStmt *letstmt = op->body.as<LetStmt>();
            if (letstmt && letstmt->name != op->name) {
                Stmt s = new LetStmt(letstmt->name, letstmt->value, new For(op, op->min, op->extent, letstmt->body));
                stmt = mutate(s);
                return;
            }
        }
        IRMutator::visit(op);
    }

    virtual void visit(const Store *op) {
        IRMutator::visit(op);
    }

    virtual void visit(const Provide *op) {
        IRMutator::visit(op);
    }

    virtual void visit(const Allocate *op) {
        IRMutator::visit(op);
    }

    virtual void visit(const Realize *op) {
        IRMutator::visit(op);
    }

    virtual void visit(const Block *op) {        
        IRMutator::visit(op);
    }    
};

Expr simplify(Expr e) {
    return Simplify().mutate(e);
}

Stmt simplify(Stmt s) {
    return Simplify().mutate(s);
}

Stmt simplify_undef(Stmt s) { 
    return s.defined() ? simplify(s) : s; 
}
Expr simplify_undef(Expr e) { 
    return e.defined() ? simplify(e) : e; 
}

bool proved(Expr e, bool &disproved) {
    Expr b = Simplify().mutate(e);
    bool result = is_one(b);
    disproved = is_zero(b);
    log logger(2);
    logger << "Attempt to prove  " << e << "\n  ==>  ";
    if (equal(e,b)) logger << "same  ==>  ";
    else logger << b << "\n  ==>  ";
    logger << (result ? "true" : "false") << "\n";
    return result;
}

bool proved(Expr e) {
    bool dummy;
    return proved(e, dummy);
}

namespace{
void check(Expr a, Expr b) {
    Expr simpler = simplify(a);
    if (!equal(simpler, b)) {
        std::cout << std::endl << "Simplification failure: " << std::endl;
        std::cout << "Input: " << a << std::endl;
        std::cout << "Output: " << simpler << std::endl;
        std::cout << "Expected output: " << b << std::endl;
        assert(false);
    }
}

void check_proved(Expr e) {
    if (proved(e)) return;
    std::cout << "Could not prove: " << e << std::endl;
    std::cout << "Simplified: " << simplify(e) << std::endl;
    assert(false);
}
}

        
void simplify_test() {
    Expr x = Var("x"), y = Var("y"), z = Var("z");
    Expr xf = cast<float>(x);
    Expr yf = cast<float>(y);
    
    // Check the type casting operations.
    assert((int_cast_constant(Int(8), 128) == (int8_t) 128) && "Simplify test failed: int_cast_constant");
    assert((int_cast_constant(UInt(8), -1) == (uint8_t) -1) && "Simplify test failed: int_cast_constant");
    assert((int_cast_constant(Int(16), 65000) == (int16_t) 65000) && "Simplify test failed: int_cast_constant");
    assert((int_cast_constant(UInt(16), 128000) == (uint16_t) 128000) && "Simplify test failed: int_cast_constant");
    assert((int_cast_constant(UInt(16), -53) == (uint16_t) -53) && "Simplify test failed: int_cast_constant");
    assert((int_cast_constant(UInt(32), -53) == (int)((uint32_t) -53)) && "Simplify test failed: int_cast_constant");
    assert((int_cast_constant(Int(32), -53) == -53) && "Simplify test failed: int_cast_constant");


    check(new Cast(Int(32), new Cast(Int(32), x)), x);
    check(new Cast(Float(32), 3), 3.0f);
    check(new Cast(Int(32), 5.0f), 5);

    check(new Cast(Int(32), new Cast(Int(8), 3)), 3);
    check(new Cast(Int(32), new Cast(Int(8), 1232)), -48);
    
    // Check evaluation of constant expressions involving casts
    check(cast(UInt(16), 53) + cast(UInt(16), 87), cast(UInt(16), 140));
    check(cast(Int(8), 127) + cast(Int(8), 1), cast(Int(8), -128));
    check(cast(UInt(16), -1) - cast(UInt(16), 1), cast(UInt(16), 65534));
    check(cast(Int(16), 4) * cast(Int(16), -5), cast(Int(16), -20));
    check(cast(Int(16), 16) / cast(Int(16), 4), cast(Int(16), 4));
    check(cast(Int(16), 23) % cast(Int(16), 5), cast(Int(16), 3));
    check(min(cast(Int(16), 30000), cast(Int(16), -123)), cast(Int(16), -123));
    check(max(cast(Int(16), 30000), cast(Int(16), 65000)), cast(Int(16), 30000));
    check(cast(UInt(16), -1) == cast(UInt(16), 65535), const_true());
    check(cast(UInt(16), 65) == cast(UInt(16), 66), const_false());
    check(cast(UInt(16), -1) < cast(UInt(16), 65535), const_false());
    check(cast(UInt(16), 65) < cast(UInt(16), 66), const_true());
    // Specific checks for 32 bit unsigned expressions - ensure simplifications are actually unsigned.
    // 4000000000 (4 billion) is less than 2^32 but more than 2^31.  As an int, it is negative.
    check(cast(UInt(32), (int) 4000000000UL) + cast(UInt(32), 5), cast(UInt(32), (int) 4000000005UL));
    check(cast(UInt(32), (int) 4000000000UL) - cast(UInt(32), 5), cast(UInt(32), (int) 3999999995UL));
    check(cast(UInt(32), (int) 4000000000UL) / cast(UInt(32), 5), cast(UInt(32), 800000000));
    check(cast(UInt(32), 800000000) * cast(UInt(32), 5), cast(UInt(32), (int) 4000000000UL));
    check(cast(UInt(32), (int) 4000000023UL) % cast(UInt(32), 100), cast(UInt(32), 23));
    check(min(cast(UInt(32), (int) 4000000023UL) , cast(UInt(32), 1000)), cast(UInt(32), (int) 1000));
    check(max(cast(UInt(32), (int) 4000000023UL) , cast(UInt(32), 1000)), cast(UInt(32), (int) 4000000023UL));
    check(cast(UInt(32), (int) 4000000023UL) < cast(UInt(32), 1000), const_false());
    check(cast(UInt(32), (int) 4000000023UL) == cast(UInt(32), 1000), const_false());
    
    // Check some specific expressions involving div and mod
    check(Expr(23) / 4, Expr(5));
    check(Expr(-23) / 4, Expr(-6));
    check(Expr(-23) / -4, Expr(5));
    check(Expr(23) / -4, Expr(-6));
    check(Expr(-2000000000) / 1000000001, Expr(-2));
    check(Expr(23) % 4, Expr(3));
    check(Expr(-23) % 4, Expr(1));
    check(Expr(-23) % -4, Expr(-3));
    check(Expr(23) % -4, Expr(-1));
    check(Expr(-2000000000) % 1000000001, Expr(2));

    check(3 + x, x + 3);
    check(Expr(3) + Expr(8), 11);
    check(Expr(3.25f) + Expr(7.75f), 11.0f);
    check(x + 0, x);
    check(0 + x, x);
    check(Expr(new Ramp(x, 2, 3)) + Expr(new Ramp(y, 4, 3)), new Ramp(x+y, 6, 3));
    check(Expr(new Broadcast(4.0f, 5)) + Expr(new Ramp(3.25f, 4.5f, 5)), new Ramp(7.25f, 4.5f, 5));
    check(Expr(new Ramp(3.25f, 4.5f, 5)) + Expr(new Broadcast(4.0f, 5)), new Ramp(7.25f, 4.5f, 5));
    check(Expr(new Broadcast(3, 3)) + Expr(new Broadcast(1, 3)), new Broadcast(4, 3));
    check((x + 3) + 4, x + 7);
    check(4 + (3 + x), x + 7);
    check((x + 3) + y, (x + y) + 3);
    check(y + (x + 3), (y + x) + 3);
    check((3 - x) + x, 3);
    check(x + (3 - x), 3);
    check(1 - (x + 2), -1 - x); //LH
    check(1 - (x - 2), 3 - x); //LH
    check(0 - (x + -4), 4 - x); //LH
    check(x*y + x*z, x*(y+z));
    check(x*y + z*x, x*(y+z));
    check(y*x + x*z, x*(y+z));
    check(y*x + z*x, x*(y+z));

    check(x - 0, x);
    check((x/y) - (x/y), 0);
    check(x - 2, x + (-2));
    check(Expr(new Ramp(x, 2, 3)) - Expr(new Ramp(y, 4, 3)), new Ramp(x-y, -2, 3));
    check(Expr(new Broadcast(4.0f, 5)) - Expr(new Ramp(3.25f, 4.5f, 5)), new Ramp(0.75f, -4.5f, 5));
    check(Expr(new Ramp(3.25f, 4.5f, 5)) - Expr(new Broadcast(4.0f, 5)), new Ramp(-0.75f, 4.5f, 5));
    check(Expr(new Broadcast(3, 3)) - Expr(new Broadcast(1, 3)), new Broadcast(2, 3));
    check((x + y) - x, y);
    check((x + y) - y, x);
    check(x - (x + y), 0 - y);
    check(x - (y + x), 0 - y);
    check((x + 3) - 2, x + 1);
    check((x + 3) - y, (x - y) + 3);
    check((x - 3) - y, (x - y) + (-3));
    check(x - (y - 2), (x - y) + 2);
    check(3 - (y - 2), 5 - y);
    check(x*y - x*z, x*(y-z));
    check(x*y - z*x, x*(y-z));
    check(y*x - x*z, x*(y-z));
    check(y*x - z*x, x*(y-z));

    check(x*0, 0);
    check(0*x, 0);
    check(x*1, x);
    check(1*x, x);
    check(Expr(2.0f)*4.0f, 8.0f);
    check(Expr(2)*4, 8);
    check((3*x)*4, x*12);
    check(4*(3+x), x*4 + 12);
    check(Expr(new Broadcast(4.0f, 5)) * Expr(new Ramp(3.0f, 4.0f, 5)), new Ramp(12.0f, 16.0f, 5));
    check(Expr(new Ramp(3.0f, 4.0f, 5)) * Expr(new Broadcast(2.0f, 5)), new Ramp(6.0f, 8.0f, 5));
    check(Expr(new Broadcast(3, 3)) * Expr(new Broadcast(2, 3)), new Broadcast(6, 3));

    check(0/x, 0);
    check(x/1, x);
    check(x/x, 1);
    check(Expr(7)/3, 2);
    check(Expr(6.0f)/2.0f, 3.0f);
    check((x / 3) / 4, x / 12);
    check((x*4)/2, x*2);
    check((x*2)/4, x/2);
    check((x*4 + y)/2, x*2 + y/2);
    check((y + x*4)/2, y/2 + x*2);
    check((x*4 - y)/2, x*2 - y/2);
    check((y - x*4)/2, y/2 - x*2);
    check(xf / 4.0f, xf * 0.25f);
    check(Expr(new Broadcast(y, 4)) / Expr(new Broadcast(x, 4)), 
          Expr(new Broadcast(y/x, 4)));
    check(Expr(new Ramp(x, 4, 4)) / 2, new Ramp(x/2, 2, 4));

    check(Expr(7) % 2, 1);
    check(Expr(7.25f) % 2.0f, 1.25f);
    check(Expr(-7.25f) % 2.0f, 0.75f);
    check(Expr(-7.25f) % -2.0f, -1.25f);
    check(Expr(7.25f) % -2.0f, -0.75f);
    check(Expr(new Broadcast(x, 4)) % Expr(new Broadcast(y, 4)), 
          Expr(new Broadcast(x % y, 4)));
    check((x*8) % 4, 0);
    check((x*8 + y) % 4, y % 4);
    check((y + x*8) % 4, y % 4);
    check((y*16 + 13) % 2, 1);
    check(Expr(new Ramp(x, 2, 4)) % (new Broadcast(2, 4)), 
          new Broadcast(x % 2, 4));
    check(Expr(new Ramp(2*x+1, 4, 4)) % (new Broadcast(2, 4)), 
          new Broadcast(1, 4));

    check(new Min(7, 3), 3);
    check(new Min(4.25f, 1.25f), 1.25f);
    check(new Min(new Broadcast(x, 4), new Broadcast(y, 4)), 
          new Broadcast(new Min(x, y), 4));
    check(new Min(x, x+3), x);
    check(new Min(x+4, x), x);
    check(new Min(x-1, x+2), x+(-1));
    check(new Min(7, new Min(x, 3)), new Min(x, 3));
    check(new Min(new Min(x, y), x), new Min(x, y));
    check(new Min(new Min(x, y), y), new Min(x, y));
    check(new Min(x, new Min(x, y)), new Min(x, y));
    check(new Min(y, new Min(x, y)), new Min(x, y));
	check(new Min(new Max(new Min(x, 18), 7), 21), new Min(new Max(x, 7), 18));
	check(new Min(new Max(x, 5), 3), Expr(3));

    check(new Max(7, 3), 7);
    check(new Max(4.25f, 1.25f), 4.25f);
    check(new Max(new Broadcast(x, 4), new Broadcast(y, 4)), 
          new Broadcast(new Max(x, y), 4));
    check(new Max(x, x+3), x+3);
    check(new Max(x+4, x), x+4);
    check(new Max(x-1, x+2), x+2);
    check(new Max(7, new Max(x, 3)), new Max(x, 7));
    check(new Max(new Max(x, y), x), new Max(x, y));
    check(new Max(new Max(x, y), y), new Max(x, y));
    check(new Max(x, new Max(x, y)), new Max(x, y));
    check(new Max(y, new Max(x, y)), new Max(x, y));
	check(new Max(new Min(new Max(x, 5), 15), 7), new Max(new Min(x, 15), 7));
	check(new Max(new Min(x, 7), 9), Expr(9));

    Expr t = const_true(), f = const_false();
    check(x == x, t);
    check(x == (x+1), f);
    check(x-2 == y+3, x == y+5);
    check(x+y == y+z, x == z);
    check(y+x == y+z, x == z);
    check(x+y == z+y, x == z);
    check(y+x == z+y, x == z);
    check((y+x)*17 == (z+y)*17, x == z);
    check(x*0 == y*0, t);
    check(x == x+y, y == 0);
    check(x+y == x, y == 0);

    check(x < x, f);
    check(x < (x+1), t);
    check(x-2 < y+3, x < y+5);
    check(x+y < y+z, x < z);
    check(y+x < y+z, x < z);
    check(x+y < z+y, x < z);
    check(y+x < z+y, x < z);
    check((y+x)*17 < (z+y)*17, x < z);
    check(x*0 < y*0, f);
    check(x < x+y, 0 < y);
    check(x+y < x, y < 0);
    
    //LH
    check(new LT(new Ramp(0, 1, 8), new Broadcast(8, 8)), const_true(8));
    check(new GT(new Ramp(0, -1, 8), new Broadcast(1, 8)), const_false(8));
    check(new Min(new Ramp(0, 1, 8), new Ramp(2, 1, 8)), new Ramp(0, 1, 8));
    check(new Min(new Ramp(0, 1, 8), new Broadcast(0, 8)), new Broadcast(0, 8));
    check(new Max(new Ramp(0, 1, 8), new Ramp(2, 1, 8)), new Ramp(2, 1, 8));
    check(new Max(new Ramp(0, 1, 8), new Broadcast(0, 8)), new Ramp(0, 1, 8));
    check(new Max(new Ramp(0, 1, 8), new Ramp(2, 1, 8)), new Ramp(2, 1, 8));
    check(new Max(new Ramp(0, 1, 8), new Broadcast(1, 8)), 
            new Max(new Ramp(0, 1, 8), new Broadcast(1, 8)));
            
 
    check(select(x < 3, 2, 2), 2);
    check(select(x < (x+1), 9, 2), 9);
    check(select(x > (x+1), 9, 2), 2);
    // Selects of comparisons should always become selects of LT or selects of EQ
    check(select(x != 5, 2, 3), select(x == 5, 3, 2));    
    check(select(x >= 5, 2, 3), select(x < 5, 3, 2));    
    check(select(x <= 5, 2, 3), select(5 < x, 3, 2));    
    check(select(x > 5, 2, 3), select(5 < x, 2, 3));    

    // Check that simplifier can recognise instances where the extremes of the
    // datatype appear as constants in comparisons, Min and Max expressions.
    // The result of min/max with extreme is known to be either the extreme or
    // the other expression.  The result of < or > comparison is known to be true or false.
    check(x <= Int(32).max(), const_true());
    check(new Cast(Int(16), x) >= Int(16).min(), const_true());
    check(x < Int(32).min(), const_false());
    check(new Min(new Cast(UInt(16), x), new Cast(UInt(16), 65535)), new Cast(UInt(16), x));
    check(new Min(x, Int(32).max()), x);
    check(new Min(Int(32).min(), x), Int(32).min());
    check(new Max(new Cast(Int(8), x), new Cast(Int(8), -128)), new Cast(Int(8), x));
    check(new Max(x, Int(32).min()), x);
    check(new Max(x, Int(32).max()), Int(32).max());
    // Check that non-extremes do not lead to incorrect simplification
    check(new Max(new Cast(Int(8), x), new Cast(Int(8), -127)), new Max(new Cast(Int(8), x), new Cast(Int(8), -127)));

    check(!f, t);
    check(!t, f);
    check(!(x < y), y <= x);
    check(!(x > y), x <= y);
    check(!(x >= y), x < y);
    check(!(x <= y), y < x);
    check(!(x == y), x != y);
    check(!(x != y), x == y);
    check(!(!(x == 0)), x == 0);
    check(!Expr(new Broadcast(x > y, 4)), 
          new Broadcast(x <= y, 4));

    check(t && (x < 0), x < 0);
    check(f && (x < 0), f);
    check(t || (x < 0), t);
    check(f || (x < 0), x < 0);

    Expr vec = new Variable(Int(32, 4), "vec");
    // Check constants get pushed inwards
    check(new Let("x", 3, x+4), new Let("x", 3, 7));

    // Check ramps in lets get pushed inwards
    check(new Let("vec", new Ramp(x*2+7, 3, 4), vec + Expr(new Broadcast(2, 4))), 
          new Let("vec.base.0", x*2+7, 
                  new Let("vec", new Ramp(x*2+7, 3, 4), 
                          new Ramp(Expr(new Variable(Int(32), "vec.base.0")) + 2, 3, 4))));

    // Check broadcasts in lets get pushed inwards
    check(new Let("vec", new Broadcast(x, 4), vec + Expr(new Broadcast(2, 4))),
          new Let("vec.value.1", x, 
                  new Let("vec", new Broadcast(x, 4), 
                          new Broadcast(Expr(new Variable(Int(32), "vec.value.1")) + 2, 4))));
    // Check values don't jump inside lets that share the same name
    check(new Let("x", 3, Expr(new Let("x", y, x+4)) + x), 
          new Let("x", 3, Expr(new Let("x", y, y+4)) + 3));

    check(new Let("x", y, Expr(new Let("x", y*17, x+4)) + x), 
          new Let("x", y, Expr(new Let("x", y*17, x+4)) + y));
          
    check_proved(Expr(new Min(new Max(x, 1), 10)) <= 10);
    check_proved(Expr(new Min(new Max(x, 1), 10)) >= 1);
    check_proved(Expr(new Min(x, 1953)) + -2 + -1 <= x + -1);

    std::cout << "Simplify test passed" << std::endl;
}
}
}


# include "SolverCpp.h"
