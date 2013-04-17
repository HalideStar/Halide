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
    
    virtual void visit(const TargetVar *op) {
        // Target variable named in the node is added to the targets.
        targets.push_back(op->var);
        Expr e = mutate(op->e);
        if (! e.same_as(op->e)) expr = new TargetVar(op->var, e);
        else expr = op;
        targets.pop_back(); // Remove the target for processing above
    }
    
    virtual void visit(const StmtTargetVar *op) {
        // Target variable named in the node is added to the targets.
        targets.push_back(op->var);
        Stmt s = mutate(op->s);
        if (! s.same_as(op->s)) stmt = new StmtTargetVar(op->var, s);
        else stmt = op;
        targets.pop_back(); // Remove the target for processing above
    }
    
    virtual void visit(const Solve *op) {
        log(3) << depth << " Solve simplify " << Expr(op) << "\n";
        Expr e = mutate(op->e);
        
        log(3) << depth << " Solve using " << e << "\n";
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
        } else if (e.same_as(op->e)) {
            expr = op; // Nothing more to do.
        } else {
            expr = solve(e, op->v);
        }
        log(3) << depth << " Solve simplified to " << expr << "\n";
    }
    
    
    //virtual void visit(const IntImm *op) {
    //virtual void visit(const FloatImm *op) {
    //virtual void visit(const Cast *op) {
    //virtual void visit(const Variable *op) {
    
    virtual void visit(const Add *op) {
        log(3) << depth << " XAdd simplify " << Expr(op) << "\n";
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
        log(3) << depth << " XAdd simplified to " << expr << "\n";
    }

    virtual void visit(const Sub *op) {
        log(3) << depth << " XSub simplify " << Expr(op) << "\n";
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
        log(3) << depth << " XSub simplified to " << expr << "\n";
    }

    virtual void visit(const Mul *op) {
        log(3) << depth << " XMul simplify " << Expr(op) << "\n";
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
        log(3) << depth << " XMul simplified to " << expr << "\n";
    }

    virtual void visit(const Div *op) {
        log(3) << depth << " XDiv simplify " << Expr(op) << "\n";
        Expr a = mutate(op->a), b = mutate(op->b);
        
        const Mul *mul_a = a.as<Mul>();
        const Add *add_a = a.as<Add>();
        const Sub *sub_a = a.as<Sub>();
        const Div *div_a = a.as<Div>();
        if (mul_a && equal(mul_a->b, b) && is_constant_expr(b)) {
            // aa * k / k.  Eliminate k assuming k != 0.
            expr = mul_a->a;         
        } else if (mul_a && equal(mul_a->a, b) && is_constant_expr(b)) {
            // k * aa / k.  Eliminate k assuming k != 0.
            expr = mul_a->b;    
        } else if (add_a && equal(add_a->b, b) && is_constant_expr(b)) {
            // (e + k) / k --> (e / k) + 1
            expr = mutate(add_a->a / b + make_one(b.type()));
        } else if (add_a && equal(add_a->a, b) && is_constant_expr(b)) {
            // (k + e) / k --> (e / k) + 1
            expr = mutate(add_a->b / b + make_one(b.type()));
        } else if (sub_a && equal(sub_a->b, b) && is_constant_expr(b)) {
            // (e - k) / k --> (e / k) - 1
            expr = mutate(sub_a->a / b - make_one(b.type()));
        } else if (sub_a && equal(sub_a->a, b) && is_constant_expr(b)) {
            // (k - e) / k --> 1 - (e / k)
            expr = mutate(make_one(b.type()) - sub_a->b / b);
        } else if (div_a && is_constant_expr(div_a->b) && ! is_constant_expr(b)) {
            // (aa / kab) / vb --> (aa / vb) / kab
            expr = mutate((div_a->a / b) / div_a->b);
        } else {
            Simplify::visit(op);
        }
        log(3) << depth << " XDiv simplified to " << expr << "\n";
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
    
# if 0
    // We could be more aggressive about inserting let expressions for Solve
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
};

Stmt solver(Stmt s) {
    return Solver().mutate(s);
}

Expr solver(Expr e) {
    return Solver().mutate(e);
}

namespace {
// Method to check the operation of the Solver class.
// This is the core class, simplifying expressions that include Target and Solve.
void checkSolver(Expr a, Expr b) {
    Solver s;
    s.targets.push_back("x");
    s.targets.push_back("y");
    Expr r = s.mutate(a);
    if (!equal(b, r)) {
        std::cout << std::endl << "Solve failure: " << std::endl;
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
    checkSolver(solve((x * d) / d, Interval(1,17)), solve(x, Interval(1,17)));
    checkSolver(solve((x * d + d) / d, Interval(1,17)), solve(x, Interval(0,16)) + 1);
    checkSolver(solve((x * d - d) / d, Interval(1,17)), solve(x, Interval(2,18)) + -1);
    
    checkSolver(solve(x + 4, Interval(0,new Infinity(+1))), solve(x, Interval(-4,new Infinity(+1))) + 4);
    checkSolver(solve(x + 4, Interval(new Infinity(-1),10)), solve(x, Interval(new Infinity(-1),6)) + 4);
    
    // A few complex expressions
    checkSolver(solve(x + c + 2 * y + d, Interval(0,10)), solve(x + y * 2, Interval(0 - d - c, 10 - d - c)) + c + d);
    // Solve 0 <= x + 10 + x + 15 <= 10
    // -25 <= x * 2 <= -15
    // -12 <= x <= -8
    checkSolver(solve(x + 10 + x + 15, Interval(0,10)), solve(x, Interval(-12, -8)) * 2 + 25);
    
    std::cout << "Solve test passed" << std::endl;
}

// end namespace Internal
}
}
