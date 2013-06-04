#include "Solver.h"
#include "InfInterval.h"
#include "BoundsAnalysis.h"

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



// TO BE DEPRECATED...
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
    
    bool find(const std::string &name) {
        for (size_t i = 0; i < varlist.size(); i++) {
            if (varlist[i] == name) {
                return true;
            }
        }
        return false;
    }

    void visit(const Variable *op) {
        if (result) return; // Once one is found, no need to find more.
        // Check whether variable name is in the list of known names.
        result = find(op->name);
    }

    // If a Let node redefines one of the variables that we are solving for,
    // then the variable inside the Let is not the same variable.
    void visit(const Let *op) {
        if (result) return; // Do not continue checking once one variable is found.
        
        // If the value expression of the Let contains the variable, then
        // we have found the variable.
        op->value.accept(this);
        
        // The name might be hidden within the body of the let, in
        // which case drop it from the list.
        if (find(op->name)) {
            // Make a new vector of name strings, copy excluding the match,
            // and use it in the body.
            std::vector<std::string> newlist;
            for (size_t i = 0; i < varlist.size(); i++) {
                if (varlist[i] != op->name) {
                    newlist.push_back(varlist[i]);
                }
            }
            HasVariable newsearcher(newlist);
            op->body.accept(&newsearcher);
            result = newsearcher.result;
        } else {
            op->body.accept(this);
        }
    }
};

// is_constant_expr: Determine whether an expression is constant relative to a list of free variables
// that may or may not occur in the expression.
bool is_constant_expr(std::vector<std::string> varlist, Expr e) {
    // Walk the expression; if no variables in varlist are found then it is a constant expression
    HasVariable hasvar(varlist);
    e.accept(&hasvar);
    return ! hasvar.result;
}


// HasTarget walks an expression and determines whether it contains any 
// of the current target variables.  Call the method is_constant_expr() to get
// your answer.

class HasTarget : public IRLazyScopeProcess {
private:
    int search_context;

public:
    bool result;
    HasTarget() : search_context(0), result(false) {}
    
    bool is_constant_expr(Expr e) {
        result = false;
        search_context = current_context();
        process(e);
        //if (result) std::cout << "Expression is not constant: " << e << "\n";
        //else std::cout << "Expression is constant: " << e << "\n";
        return ! result;
    }
    
private:
    using IRLazyScope::visit;
    
    // Override the process methods so that search can be terminated once successful.
    virtual void process(const Stmt &stmt) { if (result) return; IRLazyScope::process(stmt); }
    virtual void process(const Expr &expr) { if (result) return; IRLazyScope::process(expr); }
    
    void visit(const Variable *op) {
        if (result) return; // Once one is found, no need to find more.
        // Check whether variable name is in the list of known names.
        result = is_target(op->name, search_context);
        //if (result) std::cout << "Expression is not constant: " << op->name << "\n";
    }
};

namespace {
// Convenience methods for building solve nodes.
Expr solve(Expr e, InfInterval i) {
    return new Solve(e, i);
}

Expr solve(Expr e, std::vector<InfInterval> i) {
    return new Solve(e, i);
}

// Apply unary operator to a vector of InfInterval by applying it to each InfInterval
inline std::vector<InfInterval> v_apply(InfInterval (*f)(InfInterval), std::vector<InfInterval> v) {
    std::vector<InfInterval> result;
    for (size_t i = 0; i < v.size(); i++) {
        result.push_back((*f)(v[i]));
    }
    return result;
}

// Apply binary operator to a vector of InfInterval by applying it to each InfInterval
inline std::vector<InfInterval> v_apply(InfInterval (*f)(InfInterval, Expr), std::vector<InfInterval> v, Expr b) {
    std::vector<InfInterval> result;
    for (size_t i = 0; i < v.size(); i++) {
        result.push_back((*f)(v[i], b));
    }
    return result;
}

// Apply binary operator to a vector of InfInterval by applying it to each InfInterval
inline std::vector<InfInterval> v_apply(InfInterval (*f)(InfInterval, InfInterval), std::vector<InfInterval> v, InfInterval w) {
    std::vector<InfInterval> result;
    for (size_t i = 0; i < v.size(); i++) {
        result.push_back((*f)(v[i], w));
    }
    return result;
}

// Apply binary operator to a vector of InfInterval by applying it to each InfInterval
inline std::vector<InfInterval> v_apply(InfInterval (*f)(InfInterval, InfInterval), std::vector<InfInterval> u, std::vector<InfInterval> v) {
    std::vector<InfInterval> result;
    assert(u.size() == v.size() && "Vectors of intervals of different sizes");
    for (size_t i = 0; i < v.size(); i++) {
        result.push_back((*f)(u[i], v[i]));
    }
    return result;
}

// Apply binary operator to a vector of InfInterval by applying it to each InfInterval, with additional int parameter
inline std::vector<InfInterval> v_apply(InfInterval (*f)(InfInterval, InfInterval, int), std::vector<InfInterval> v, InfInterval w, int k) {
    std::vector<InfInterval> result;
    for (size_t i = 0; i < v.size(); i++) {
        result.push_back((*f)(v[i], w, k));
    }
    return result;
}

InfInterval inverseMin(InfInterval v, Expr k) {
    // inverse of min on an interval against constant expression k.
    // If v.max >= k then the Min ensures that the upper bound is in
    // the target interval, so the new max is +infinity; otherwise
    // the new max is v.max.
    return InfInterval(v.min, simplify(select(v.max >= k, make_infinity(v.max.type(), +1), v.max)));
}

InfInterval inverseMin(InfInterval v, InfInterval k) {
    // inverse of min on an interval against constant expression k.
    // If v.max >= k.max then the Min ensures that the upper bound is in
    // the target interval, so the new max is +infinity; otherwise
    // the new max is v.max.
    return InfInterval(v.min, simplify(select(v.max >= k.max, make_infinity(v.max.type(), +1), v.max)));
}

InfInterval inverseMax(InfInterval v, Expr k) {
    // inverse of max on an interval against constant expression k.
    // If v.min <= k then the Max ensures that the lower bound is in
    // the target interval, so the new min is -infinity; otherwise
    // the new min is v.min.
    return InfInterval(simplify(select(v.min <= k, make_infinity(v.min.type(), -1), v.min)), v.max);
}

InfInterval inverseMax(InfInterval v, InfInterval k) {
    // inverse of max on an interval against constant expression k.
    // If v.min <= k.min then the Max ensures that the lower bound is in
    // the target interval, so the new min is -infinity; otherwise
    // the new min is v.min.
    // The new max is v.max.
    return InfInterval(simplify(select(v.min <= k.min, make_infinity(v.min.type(), -1), v.min)), v.max);
}

InfInterval inverseRamp(InfInterval v, InfInterval s, int width) {
    // Compute an interval such that a ramp with base in the result interval
    // and designated stride interval always results in an interval no bigger than v.
    // v will in fact be a vector interval but the result is not a vector interval.
    assert(v.min.type().width == width && v.max.type().width == width && "inverseRamp applied to non-vector interval");
    // Because v is a vector interval, it is likely to appear as a ramp or broadcast.
    // stride is not a vector interval.
    const Ramp *ramp_vmin = v.min.as<Ramp>();
    const Broadcast *broadcast_vmin = v.min.as<Broadcast>();
    const Ramp *ramp_vmax = v.max.as<Ramp>();
    const Broadcast *broadcast_vmax = v.max.as<Broadcast>();
    // Suppose that (Ramp(a,p),Ramp(b,q)) is the interval of v.
    // Given stride interval s == (c,d) we want to determine
    // the interval for base == (m,n).
    // Now, the interval of a ramp is given as follows in BoundsAnalysis.h:
    // Ramp(a,p) = Ramp(m,c)  Ramp(b,q) = Ramp(n,d)  
    // This follows from the meaning of stride and base.
    // It generates a "low" ramp and a "high" ramp.
    
    // basemin and basemax correspond to a and b in the above.
    // stridemin and stridemax correspond to p and q.
    Expr basemin, basemax, stridemin, stridemax;
    if (ramp_vmin) { basemin = ramp_vmin->base; stridemin = ramp_vmin->stride; }
    else if (broadcast_vmin) { basemin = broadcast_vmin->value; stridemin = make_zero(basemin.type()); }
    else if (infinity_count(v.min) != 0) { basemin = make_infinity(v.min.type().element_of(), infinity_count(v.min)); stridemin = make_infinity(v.min.type().element_of(), infinity_count(v.min)); }
    if (ramp_vmax) { basemax = ramp_vmax->base; stridemax = ramp_vmax->stride; }
    else if (broadcast_vmax) { basemax = broadcast_vmax->value; stridemax = make_zero(basemax.type()); }
    else if (infinity_count(v.max) != 0) { basemax = make_infinity(v.max.type().element_of(), infinity_count(v.max)); stridemax = make_infinity(v.max.type().element_of(), infinity_count(v.max)); }
    
    // From the above, we can see one immediate solution.
    // If p == c and  q == d then a = m and b = n.
    if (equal(stridemin, s.min) && equal(stridemax, s.max)) {
        return InfInterval(basemin, basemax);
    }
    
    // The general solution requires that Ramp(a,p) <= Ramp(m,c) for all indices in the vector
    // and that Ramp(b,q) >= Ramp(n,d) for all indices.
    // Ramp(a,p) <= Ramp(m,c) means a <= m and a + p * (w-1) <= m + c * (w-1).
    // Solving for m we get: m >= a and m >= a + p * (w-1) - c * (w-1)
    // i.e. m = min(a, a + (p - c) * (w-1));
    return InfInterval(simplify(max(basemin, basemin + (stridemin - s.min) * (width-1))),
                       simplify(min(basemax, basemax + (stridemax - s.max) * (width-1))));
}

// end anonymous namespace
}

class Solver : public Simplify {

    // using parent::visit indicates that
    // methods of the base class with different parameters
    // but the same name are not hidden, they are overloaded.
    using Simplify::visit;
    
    HasTarget has_target;
    
    BoundsAnalysis bounds;
    
    bool is_constant_expr(Expr e) { return has_target.is_constant_expr(e); }
    
    // use our own cache for IRCacheMutator.
    // Each different mutator must have its own cache.
    // Simplify uses a shared cache, so we need to override that.
    IRCacheMutator::Cache solver_cache;

public:

    // debug tracing
    int loglevel;

    Solver() : Simplify(solver_cache), loglevel(3) {}

protected:
    bool equal_bounds(Expr k) {
        InfInterval b = bounds.bounds(k);
        return equal(b.min, b.max);
    }
    
    virtual void visit(const Solve *op) {
        log(loglevel) << depth << " Solve simplify " << Expr(op) << "\n";
        Expr e = mutate(op->body);
        
        log(loglevel) << depth << " Solve using " << e << "\n";
        //const Solve *solve_e = e.as<Solve>();
        const Add *add_e = e.as<Add>();
        const Sub *sub_e = e.as<Sub>();
        const Mul *mul_e = e.as<Mul>();
        const Div *div_e = e.as<Div>();
        const Min *min_e = e.as<Min>();
        const Max *max_e = e.as<Max>();
        const Ramp *ramp_e = e.as<Ramp>();
        
        //if (solve_e) {
            // solve(solve(e)) --> solve(e) on intersection of intervals.
            // This is one approach to combining solutions, and makes it easier
            // to match them and pick them out.
            //expr = mutate(solve(solve_e->e, v_apply(intersection, op->v, solve_e->v)));
        //} else 
        if (add_e && is_constant_expr(add_e->b)) {
            //expr = mutate(solve(add_e->a, v_apply(operator-, op->v, add_e->b)) + add_e->b);
            expr = mutate(solve(add_e->a, v_apply(inverseAdd, op->v, bounds.bounds(add_e->b))) + add_e->b);
        } else if (sub_e && is_constant_expr(sub_e->b)) {
            //expr = mutate(solve(sub_e->a, v_apply(operator+, op->v, sub_e->b)) - sub_e->b);
            expr = mutate(solve(sub_e->a, v_apply(inverseSub, op->v, bounds.bounds(sub_e->b))) - sub_e->b);
        } else if (sub_e && is_constant_expr(sub_e->a)) {
            // solve(k - v) --> -solve(v - k) with interval negated
            expr = mutate(-solve(sub_e->b - sub_e->a, v_apply(operator-, op->v)));
        } else if (mul_e && is_constant_expr(mul_e->b) && equal_bounds(mul_e->b)) {
            // solve(v * k) on (a,b) --> solve(v) * k with interval (ceil(a/k), floor(b/k))
            // i.e. For integer types, find all the integers that could be multiplied back up
            // and still be in the range (a,b).  Decimate does this.
            // DOES NOT HANDLE AN INTERVAL FOR THE CONSTANT EXPRESSION
            expr = mutate(solve(mul_e->a, v_apply(decimate, op->v, mul_e->b)) * mul_e->b);
        } else if (div_e && is_constant_expr(div_e->b) && equal_bounds(div_e->b)) {
            // solve(v / k) on (a,b) --> solve(v) / k with interval a * k, b * k + (k +/- 1)
            // For integer types, find all the expanded intervals - all the integers that would
            // divide back down to the range (a,b).  Zoom does this.
            // DOES NOT HANDLE AN INTERVAL FOR THE CONSTANT EXPRESSION
            expr = mutate(solve(div_e->a, v_apply(zoom, op->v, div_e->b)) / div_e->b);
        } else if (min_e && is_constant_expr(min_e->a)) {
            // Min, Max: push outside of Solve nodes.
            // solve(min(k,v)) on (a,b) --> min(k,solve(v)). 
            expr = mutate(new Min(min_e->a, solve(min_e->b, v_apply(inverseMin, op->v, bounds.bounds(min_e->a)))));
        } else if (min_e && is_constant_expr(min_e->b)) {
            // solve(min(v,k)) on (a,b) --> min(solve(v),k). 
            expr = mutate(new Min(solve(min_e->a, v_apply(inverseMin, op->v, bounds.bounds(min_e->b))), min_e->b));
        } else if (max_e && is_constant_expr(max_e->a)) {
            // solve(max(k,v)) on (a,b) --> max(k,solve(v)). 
            expr = mutate(new Max(max_e->a, solve(max_e->b, v_apply(inverseMax, op->v, bounds.bounds(max_e->a)))));
        } else if (max_e && is_constant_expr(max_e->b)) {
            // solve(max(v,k)) on (a,b) --> max(solve(v),k). 
            expr = mutate(new Max(solve(max_e->a, v_apply(inverseMax, op->v, bounds.bounds(max_e->b))), max_e->b));
        } else if (ramp_e && is_constant_expr(ramp_e->stride)) {
            // solve(ramp(v,k,w)) on (a,b)
            //std::cout << "Solve " << e << "\n";
            //std::cout << "ramp_e->base " << ramp_e->base << "\n";
            //std::cout << "ramp_e->stride " << ramp_e->stride << "\n";
            //std::cout << "op->v " << op->v << "\n";
            //std::cout << "bounds(ramp_e->stride) " << bounds.bounds(ramp_e->stride) << "\n";
            expr = mutate(new Ramp(solve(ramp_e->base, 
                                         v_apply(inverseRamp, op->v, bounds.bounds(ramp_e->stride), ramp_e->width)),
                                   ramp_e->stride, ramp_e->width));
            //std::cout << "Solution " << expr << "\n";
        } else if (e.same_as(op->body)) {
            expr = op; // Nothing more to do.
        } else {
            expr = solve(e, op->v);
        }
        log(loglevel) << depth << " Solve simplified to " << expr << "\n";
    }
    
    
    //virtual void visit(const IntImm *op) {
    //virtual void visit(const FloatImm *op) {
    //virtual void visit(const Cast *op) {
    //virtual void visit(const Variable *op) {
    
virtual void visit(const Add *op) {
        log(loglevel) << depth << " XAdd simplify " << Expr(op) << "\n";
        Expr a = mutate(op->a), b = mutate(op->b);
        
        const Add *add_a = a.as<Add>();
        const Add *add_b = b.as<Add>();
        const Sub *sub_a = a.as<Sub>();
        const Sub *sub_b = b.as<Sub>();
        bool const_a = is_constant_expr(a);
        bool const_b = is_constant_expr(b);

        // The default behavior of simplify pushes constant values towards the RHS.
        // We want to preserve that behavior because it results in constants being
        // simplified.  On top of that, however, we want to push constant variables
        // and expressions consisting only of constant variables outside of expressions
        // consisting of/containing target variables.
        
        // Rules that are not included below and why.
        // Swap a with b when a is a constant expr and b is not.  This is not done because Simplify sometimes swaps
        // things around and there can arise a conflict leading to infinite recursion of rules.
        
        // In the following comments, k... denotes constant expression, v... denotes target variable.
        if (const_a && const_b) {
            Simplify::visit(op); // Pure constant expressions get simplified in the normal way
        } else if (add_a && !const_b && !const_a && is_constant_expr(add_a->b)) {
            // (vaa + kab) + vb --> (vaa + vb) + kab
            expr = mutate((add_a->a + b) + add_a->b);
        } else if (add_a && !const_b && !const_a && is_constant_expr(add_a->a)) {
            // (kaa + vab) + vb --> (vab + vb) + kaa
            expr = mutate((add_a->b + b) + add_a->a);
        } else if (add_b && !const_a && !const_b && is_constant_expr(add_b->b)) {
            // va + (vba + kbb) --> (va + vba) + kbb
            expr = mutate((a + add_b->a) + add_b->b);
        } else if (add_b && !const_a && !const_b && is_constant_expr(add_b->a)) {
            // va + (kba + vbb) --> (va + vbb) + kba
            expr = mutate((a + add_b->b) + add_b->a);
        } else if (sub_a && !const_b && !const_a && is_constant_expr(sub_a->a)) {
            // (kaa - vab) + vb --> (vb - vab) + kaa
            expr = mutate((b - sub_a->b) + sub_a->a);
        } else if (sub_a && !const_b && !const_a  && is_constant_expr(sub_a->b)) {
            // (vaa - kab) + vb --> (vaa + vb) - kab
            expr = mutate((sub_a->a - b) + sub_a->b);
        } else if (sub_b && !const_b && !const_a && is_constant_expr(sub_b->a)) {
            // va + (kba - vbb) --> (va - vbb) + kba
            expr = mutate((a - sub_b->b) + sub_b->a);
        } else if (sub_b && !const_b && !const_a && is_constant_expr(sub_b->b)) {
            // va + (vba - kbb) --> (va + vba) - kbb
            expr = mutate((a + sub_b->a) - sub_b->b);
        } else {
            // Adopt default behavior
            // Take care in the above rules to ensure that they do not produce changes that
            // are reversed by Simplify::visit(Add)
            Simplify::visit(op);
        }
        log(loglevel) << depth << " XAdd simplified to " << expr << "\n";
    }

    virtual void visit(const Sub *op) {
        log(loglevel) << depth << " XSub simplify " << Expr(op) << "\n";
        Expr a = mutate(op->a), b = mutate(op->b);
        
        // Override default behavior.
        // Push constant expressions outside of variable expressions.
        
        const Add *add_a = a.as<Add>();
        const Add *add_b = b.as<Add>();
        const Sub *sub_a = a.as<Sub>();
        const Sub *sub_b = b.as<Sub>();
        bool const_a = is_constant_expr(a);
        bool const_b = is_constant_expr(b);
        
        // In the following comments, k... denotes constant expression, v... denotes target variable.
        if (const_a && const_b) {
            Simplify::visit(op); // Pure constant expressions get simplified in the normal way
        //} else if (const_a || const_b) {
            // One side is constant and the other is not.  Do not rearrange this node.
        //    expr = op;
        } else if (add_a && !const_b && !const_a && is_constant_expr(add_a->b)) {
            // (vaa + kab) - vb --> (vaa - vb) + kab
            expr = mutate((add_a->a - b) + add_a->b);
        } else if (add_b && !const_b && !const_a && is_constant_expr(add_b->b)) {
            // va - (vba + kbb) --> (va - vba) - kbb
            expr = mutate((a - add_b->a) - add_b->b);
        } else if (sub_a && !const_b && !const_a && is_constant_expr(sub_a->a)) {
            // (kaa - vab) - vb --> kaa - (vab + vb)
            expr = mutate(sub_a->a - (b + sub_a->b));
        } else if (sub_a && !const_b && !const_a && is_constant_expr(sub_a->b)) {
            // (vaa - kab) - vb --> (vaa - vb) - kab
            expr = mutate((sub_a->a - b) - sub_a->b);
        } else if (sub_b && !const_b && !const_a && is_constant_expr(sub_b->a)) {
            // va - (kba - vbb) --> (va + vbb) - kba
            expr = mutate((a + sub_b->b) - sub_b->a);
        } else if (sub_b && !const_b && !const_a && is_constant_expr(sub_b->b)) {
            // va - (vba - kbb) --> (va - vba) + kbb;
            expr = mutate((a - sub_b->a) + sub_b->b);
        } else {
            // Adopt default behavior
            // Take care in the above rules to ensure that they do not produce changes that
            // are reversed by Simplify::visit(Add)
            Simplify::visit(op);
        }
        log(loglevel) << depth << " XSub simplified to " << expr << "\n";
    }

    virtual void visit(const Mul *op) {
        log(loglevel) << depth << " XMul simplify " << Expr(op) << "\n";
        Expr a = mutate(op->a), b = mutate(op->b);

        const Mul *mul_a = a.as<Mul>();
        const Mul *mul_b = b.as<Mul>();
        const Div *div_a = a.as<Div>();
        const Div *div_b = b.as<Div>();
        bool const_a = is_constant_expr(a);
        bool const_b = is_constant_expr(b);
        bool integer_types = op->type.is_int() || op->type.is_uint();
        
        // Some rules that are not included below and why
        // Swapping constant expr to RHS - in case conflict with Simplify
        // (vaa + kab) * kb  -->  (vaa * kb) + (kab * kb)
        //      No benefit. Solver can pull multiply and then add outside of a Solve node.
        // (vaa + kab) * vb  -->  (vaa * vb) + (kab * vb)
        //      No benefit. There are still two output terms to be solved simultaneously.
        
        if (const_a && const_b) {
            Simplify::visit(op); // Pure constant expressions get simplified in the normal way
        } else if (div_a && equal(div_a->b, b)) {
            // Rules to simplify multiplication combined with division.
            // (aa / b) * b  -->  aa
            expr = div_a->a;
        } else if (div_b && equal(div_b->b, a)) {
            // a * (ba / a)  -->  ba
            expr = div_b->a;
        } else if (mul_a && ! const_a && ! const_b && is_constant_expr(mul_a->b)) {
            // Rules to pull out constant expressions from inside.
            // (vaa * kab) * vb  -->  (vaa * vb) * kab
            expr = mutate((mul_a->a * b) * mul_a->b);
        } else if (mul_a && ! const_a && ! const_b && is_constant_expr(mul_a->a)) {
            // Rules to pull out constant expressions from inside.
            // (kaa * vab) * vb  -->  (vab * vb) * kaa
            expr = mutate((mul_a->b * b) * mul_a->a);
        } else if (mul_b && ! const_a && ! const_b && is_constant_expr(mul_b->b)) {
            // va * (vba * kbb)  -->  (va * vba) * kbb
            expr = mutate((a * mul_b->a) * mul_b->b);
        } else if (mul_b && ! const_a && ! const_b && is_constant_expr(mul_b->a)) {
            // va * (kba * vbb)  -->  (va * vbb) * kba
            expr = mutate((a * mul_b->b) * mul_b->a);
        } else if (div_a && ! const_a && ! const_b && ! integer_types && is_constant_expr(div_a->b)) {
            // (vaa / kab) * vb  -->  (vaa * vb) / kab.  This is correct on real
            // numbers but not on integers.
            expr = mutate((div_a->a * b) / div_a->b);
        } else if (div_b && ! const_a && ! const_b && ! integer_types && is_constant_expr(div_b->b)) {
            // va * (vba / kbb)  -->  (va * vba) / kbb.  This is correct on real
            // numbers but not on integers.
            expr = mutate((a * div_b->a) / div_b->b);
        } else {
            // Simplify everything else in the normal way.
            Simplify::visit(op);
        }
        log(loglevel) << depth << " XMul simplified to " << expr << "\n";
    }

    virtual void visit(const Div *op) {
        log(loglevel) << depth << " XDiv simplify " << Expr(op) << "\n";
        Expr a = mutate(op->a), b = mutate(op->b);
        
        const Mul *mul_a = a.as<Mul>();
        const Add *add_a = a.as<Add>();
        const Sub *sub_a = a.as<Sub>();
        const Div *div_a = a.as<Div>();
        bool const_a = is_constant_expr(a);
        bool const_b = is_constant_expr(b);
        
        // In the following comments, k... denotes constant expression, v... denotes target variable.
        if (const_a && const_b) {
            Simplify::visit(op); // Pure constant expressions get simplified in the normal way
        } else if (mul_a && equal(mul_a->b, b)) {
            // Rules that help solve equations by eliminating multiply and divide
            // by the same expression.  These rules produce particular solutions
            // that assume that the expression is non-zero.  These rules are useful whether
            // the eliminated term is a constant expression or a target variable expression.
            // aa * b / b  -->  aa
            expr = mul_a->a;         
        } else if (mul_a && equal(mul_a->a, b)) {
            // b * ab / b  -->  ab
            expr = mul_a->b;    
        } else if (add_a && equal(add_a->b, b)) {
            // (aa + b) / b  -->  aa / b + 1
            expr = mutate(add_a->a / b + make_one(b.type()));
        } else if (add_a && equal(add_a->a, b)) {
            // (b + ab) / b  -->  ab / b + 1
            expr = mutate(add_a->b / b + make_one(b.type()));
        } else if (sub_a && equal(sub_a->b, b)) {
            // (aa - b) / b  -->  aa / b - 1
            expr = mutate(sub_a->a / b - make_one(b.type()));
        } else if (sub_a && equal(sub_a->a, b)) {
            // (b - ab) / b  -->  1 - ab / b
            expr = mutate(make_one(b.type()) - sub_a->b / b);
        } else if (div_a && is_constant_expr(div_a->b) && ! const_b) {
            // (aa / kab) / vb --> (aa / vb) / kab
            expr = mutate((div_a->a / b) / div_a->b);
        } else {
            Simplify::visit(op);
        }
        log(loglevel) << depth << " XDiv simplified to " << expr << "\n";
    }

    
    //virtual void visit(const Mod *op) {
    //virtual void visit(const Min *op) {
    //virtual void visit(const Max *op) {
    //virtual void visit(const EQ *op) {
    //virtual void visit(const NE *op) {
    //virtual void visit(const LT *op) {
    //virtual void visit(const LE *op) {
    //virtual void visit(const GT *op) {
    //virtual void visit(const GE *op) {
    //virtual void visit(const And *op) {
    //virtual void visit(const Or *op) {
    //virtual void visit(const Not *op) {
    //virtual void visit(const Select *op) {
    //virtual void visit(const Load *op) {
    //virtual void visit(const Ramp *op) {
    //virtual void visit(const Broadcast *op) {
    //virtual void visit(const Call *op) {
    
    // Let and LetStmt should be aggressively inlined in the pre-solver
    // pass where the Solve, TargetVar and StmtTargetVar nodes are created.
    //void visit(const Let *op) {
    //void visit(const LetStmt *op) {

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
};

Stmt solver(Stmt s) {
    return Solver().simplify(s);
}

Expr solver(Expr e) {
    return Solver().simplify(e);
}



class ExtractSolutions : public IRLazyScopeProcess {
    using IRLazyScopeProcess::visit;
public:

    std::vector<Solution> solutions;
    std::string var;
    Expr expr_source;
    Stmt stmt_source;
    
    // Extract all solutions, each with their own variable and source information
    ExtractSolutions() : var("") {}
    // Extract solutions only for a particular variable and source node
    ExtractSolutions(std::string _var, Expr expr_s, Stmt stmt_s) : 
        var(_var), expr_source(expr_s), stmt_source(stmt_s) {}

protected:

    // Extract solution from a Solve node.
    virtual void visit(const Solve *op) {
        process(op->body);
        
        // In case of nested Solve nodes, walk through them to find the variable.
        Expr body = op->body;
        while (const Solve *solve = body.as<Solve>()) {
            body = solve->body;
        }
        
        // We extract a solution only if the expression is a simple variable.
        const Variable *variable = body.as<Variable>();
        if (variable) {
# if 0
            // Find the variable among the list of target variables.
            int found = find_target(variable->name);
            if (found >= 0) {
                // Found the variable as a target. Does it match the solutions
                // that we are looking for?
                if ((! expr_source.defined() || expr_source.same_as(expr_sources[found])) &&
                    (! stmt_source.defined() || stmt_source.same_as(stmt_sources[found])) &&
                    (var == "" || var == variable->name)) {
                    solutions.push_back(Solution(targets[found], expr_sources[found], stmt_sources[found], op->v));
                }
            }
# else
            // Check whether the variable is the one we are seeking.
            if (variable->name == var) {
                // The name matches.  Now ensure that the source matches.
                int found = find_target(variable->name); // Find the defining context.
                const DefiningNode *node = call(found); // Call to the defining context.
                const TargetVar *tvar = node->node().as<TargetVar>();
                const StmtTargetVar *svar = node->node().as<StmtTargetVar>();
                if (tvar) {
                    // This is defined by a TargetVar.
                    solutions.push_back(Solution(variable->name, tvar->source, Stmt(), op->v));
                } else if (svar) {
                    solutions.push_back(Solution(variable->name, Expr(), svar->source, op->v));
                }
                ret(found);
            }
#endif
        }
    }
};

// Extract solutions where the variable name matches var and the source node is source.
std::vector<Solution> extract_solutions(std::string var, Stmt source, Stmt solved) {
    ExtractSolutions extract(var, Expr(), source);
    extract.process(solved);
    return extract.solutions;
}

std::vector<Solution> extract_solutions(std::string var, Expr source, Expr solved) {
    ExtractSolutions extract(var, source, Stmt());
    extract.process(solved);
    return extract.solutions;
}

namespace {
// Method to check the operation of the Solver class.
// This is the core class, simplifying expressions that include Target and Solve.
void checkSolver(Expr a, Expr b) {
    Solver s;
    a = new TargetVar("x", new TargetVar("y", a, Expr()), Expr());
    b = new TargetVar("x", new TargetVar("y", b, Expr()), Expr());
    Expr r = s.simplify(a);
    if (!equal(b, r)) {
        std::cout << std::endl << "Solve failure: " << std::endl;
        std::cout << "Input: " << a << std::endl;
        std::cout << "Output: " << r << std::endl;
        std::cout << "Expected output: " << b << std::endl;
        assert(false);
    }
}

void logSolver(Expr a, Expr b) {
    Solver s;
    s.loglevel = 0;
    a = new TargetVar("x", new TargetVar("y", a, Expr()), Expr());
    b = new TargetVar("x", new TargetVar("y", b, Expr()), Expr());
    Expr r = s.simplify(a);
    if (!equal(b, r)) {
        std::cout << std::endl << "Solve failure: " << std::endl;
        std::cout << "Input: " << a << std::endl;
        std::cout << "Output: " << r << std::endl;
        std::cout << "Expected output: " << b << std::endl;
        assert(false);
    }
}

void checkInverseRamp(InfInterval comb, InfInterval stride, int width, InfInterval base) {
    InfInterval test = inverseRamp(comb, stride, width);
    if (! equal(test.min, base.min) || ! equal(test.max, base.max)) {
        std::cerr << "inverseRamp failed\n";
        std::cerr << "  combined: " << comb << "\n";
        std::cerr << "  stride:   " << stride << "\n";
        std::cerr << "  expected: " << base << "\n";
        std::cerr << "  computed: " << test << "\n";
        assert(false);
    }
}
}

void solver_test() {
    Var x("x"), y("y"), c("c"), d("d"), k("k");
    
    checkInverseRamp(InfInterval(new Ramp(0,1,8), new Ramp(1,1,8)), InfInterval(1,1), 8, InfInterval(0,1));
    checkInverseRamp(InfInterval(new Ramp(0,1,8), new Ramp(3,2,8)), InfInterval(1,2), 8, InfInterval(0,3));
    checkInverseRamp(InfInterval(new Broadcast(3,8), new Broadcast(10,8)), InfInterval(0,0), 8, InfInterval(3,10));
    checkInverseRamp(InfInterval(new Broadcast(3,8), new Broadcast(10,8)), InfInterval(1,1), 8, InfInterval(3,3));
    
    checkSolver(solve(x, InfInterval(0,10)), solve(x, InfInterval(0,10)));
    checkSolver(solve(x + 4, InfInterval(0,10)), solve(x, InfInterval(-4,6)) + 4);
    checkSolver(solve(4 + x, InfInterval(0,10)), solve(x, InfInterval(-4,6)) + 4);
    checkSolver(solve(x + 4 + d, InfInterval(0,10)), solve(x, InfInterval(-4-d, 6-d)) + d + 4);
    checkSolver(solve(x - d, InfInterval(0,10)), solve(x, InfInterval(d, d+10)) - d);
    checkSolver(solve(x - (4 - d), InfInterval(0,10)), solve(x, InfInterval(4-d, 14-d)) + d + -4);
    checkSolver(solve(x - 4 - d, InfInterval(0,10)), solve(x, InfInterval(d+4, d+14)) - d + -4);
    // Solve 4-x on the interval (0,10).
    // 0 <= 4-x <= 10.
    // -4 <= -x <= 6.  solve(-x) + 4
    // 4 >= x >= -6.   -solve(x) + 4  i.e.  4 - solve(x)
    checkSolver(solve(4 - x, InfInterval(0,10)), 4 - solve(x, InfInterval(-6,4)));
    checkSolver(solve(4 - d - x, InfInterval(0,10)), 4 - (solve(x, InfInterval(-6 - d, 4 - d)) + d));
    checkSolver(solve(4 - d - x, InfInterval(0,10)) + 1, 5 - (solve(x, InfInterval(-6 - d, 4 - d)) + d));
    // Solve c - (x + d) on (0,10).
    // 0 <= c - (x + d) <= 10.
    // -c <= -(x+d) <= 10-c.
    // c >= x+d >= c-10.
    // c-d >= d >= c-d-10.
    checkSolver(solve(c - (x + d), InfInterval(0,10)), c - (solve(x, InfInterval(c-d+-10, c-d)) + d));
    
    checkSolver(solve(x * 2, InfInterval(0,10)), solve(x, InfInterval(0,5)) * 2);
    checkSolver(solve(x * 3, InfInterval(1,17)), solve(x, InfInterval(1,5)) * 3);
    checkSolver(solve(x * -3, InfInterval(1,17)), solve(x, InfInterval(-5,-1)) * -3);
    checkSolver(solve((x + 3) * 2, InfInterval(0,10)), solve(x, InfInterval(-3, 2)) * 2 + 6);
    // Solve 0 <= (x + 4) * 3 <= 10
    // 0 <= (x + 4) <= 3
    // -4 <= x <= -1
    checkSolver(solve((x + 4) * 3, InfInterval(0,10)), solve(x, InfInterval(-4, -1)) * 3 + 12);
    // Solve 0 <= (x + c) * -3 <= 10
    // 0 >= (x + c) >= -3
    // -c >= x >= -3 - c
    checkSolver(solve((x + c) * -3, InfInterval(0,10)), (solve(x, InfInterval(-3 - c, 0 - c)) + c) * -3);
    
    checkSolver(solve(x / 3, InfInterval(0,10)), solve(x, InfInterval(0, 32)) / 3);
    checkSolver(solve(x / -3, InfInterval(0,10)), solve(x, InfInterval(-32,0)) / -3);
    // Solve 1 <= (x + c) / 3 <= 17
    // 3 <= (x + c) <= 53
    // 3 - c <= x <= 53 - c
    checkSolver(solve((x + c) / 3, InfInterval(1,17)), (solve(x, InfInterval(3 - c, 53 - c)) + c) / 3);
    checkSolver(solve((x * d) / d, InfInterval(1,17)), solve(x, InfInterval(1,17)));
    checkSolver(solve((x * d + d) / d, InfInterval(1,17)), solve(x, InfInterval(0,16)) + 1);
    checkSolver(solve((x * d - d) / d, InfInterval(1,17)), solve(x, InfInterval(2,18)) + -1);
    
    checkSolver(solve(x + 4, InfInterval(0,new Infinity(Int(32), +1))), solve(x, InfInterval(-4,new Infinity(Int(32), +1))) + 4);
    checkSolver(solve(x + 4, InfInterval(new Infinity(Int(32), -1),10)), solve(x, InfInterval(new Infinity(Int(32), -1),6)) + 4);
    
    // A few complex expressions
    checkSolver(solve(x + c + 2 * y + d, InfInterval(0,10)), solve(x + y * 2, InfInterval(0 - d - c, 10 - d - c)) + c + d);
    // Solve 0 <= x + 10 + x + 15 <= 10
    // -25 <= x * 2 <= -15
    // -12 <= x <= -8
    checkSolver(solve(x + 10 + x + 15, InfInterval(0,10)), solve(x, InfInterval(-12, -8)) * 2 + 25);
    
    checkSolver(x * x, x * x);
    checkSolver(x * d, x * d);
    checkSolver(d * x, d * x);
    checkSolver((x + c) + d, (x + c) + d);
    checkSolver((x + c) + y, (x + y) + c);
    checkSolver((min(x, 1) + c) + min(y, 1), (min(x, 1) + min(y, 1)) + c);
    checkSolver((min(x, 1) + c) + min(d, 1), min(d, 1) + (min(x, 1) + c)); // Simplify reorders expression
    
    //logSolver(solve(2 * x + k + (x + 5), InfInterval(0, 100)), solve(x, InfInterval((-6-k)/3+1,(95-k)/3))*3 + k + 5);
    
    std::cout << "Solve test passed" << std::endl;
}

// end namespace Internal
}
}
