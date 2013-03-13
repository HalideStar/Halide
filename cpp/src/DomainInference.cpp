#include "IR.h"
#include "Var.h"
#include "Func.h"
#include "Image.h"
#include "IREquality.h"
#include "IROperator.h"
#include "IRVisitor.h"
#include "IRPrinter.h"
#include "log.h"
#include "Simplify.h"

using std::string;

namespace Halide { 
namespace Internal {

// VarInterval: Represents an interval associated with a variable, including
// the name of the variable and a poison flag for dead intervals.
// update computes the intersection of intervals.
void VarInterval::update(VarInterval result) {
    assert(result.varname == varname && "Trying to update incorrect variable in Domain");

    // Poison is really "inexact": the result is not assured to be correct because
    // at least one update of relevant information could not be resolved!
    // Poison set to empty expression is the same as false.  This is a hack for initialisation.
    if (! poison.defined())
        poison = result.poison;
    else if (result.poison.defined()) {
        poison = poison || result.poison;
        poison = simplify(poison);
    }

    // Update the bounds according to information newly derived.  If inexact, the
    // answers are generous (i.e. the actual domain may be smaller).
    if (! imin.defined()) { // Hack representation of infinity
        imin = result.imin;
    }
    else if (result.imin.defined()) {
        imin = max(imin, result.imin); // Update min as maximum of both expressions
        imin = simplify(imin);
    }
    if (! imax.defined()) {
        imax = result.imax;
    }
    else if (result.imax.defined()) {
        imax = min(imax, result.imax);
        imax = simplify(imax);
    }
}

// End namespace Internal
}

// Domain: Represents a domain whether valid, defined or some other.
Domain::Domain() {
    intervals.clear();
}

Domain::Domain(std::string xvarname, Expr xpoisoned, Expr xmin, Expr xmax) {
    intervals = vec(Internal::VarInterval(xvarname, xpoisoned, xmin, xmax));
}

Domain::Domain(std::string xvarname, Expr xpoisoned, Expr xmin, Expr xmax,
               std::string yvarname, Expr ypoisoned, Expr ymin, Expr ymax) {
    intervals = vec(Internal::VarInterval(xvarname, xpoisoned, xmin, xmax),
                    Internal::VarInterval(yvarname, ypoisoned, ymin, ymax));
}

Domain::Domain(std::string xvarname, Expr xpoisoned, Expr xmin, Expr xmax,
               std::string yvarname, Expr ypoisoned, Expr ymin, Expr ymax,
               std::string zvarname, Expr zpoisoned, Expr zmin, Expr zmax) {
    intervals = vec(Internal::VarInterval(xvarname, xpoisoned, xmin, xmax),
                    Internal::VarInterval(yvarname, ypoisoned, ymin, ymax),
                    Internal::VarInterval(zvarname, zpoisoned, zmin, zmax));
}

Domain::Domain(std::string xvarname, Expr xpoisoned, Expr xmin, Expr xmax,
               std::string yvarname, Expr ypoisoned, Expr ymin, Expr ymax,
               std::string zvarname, Expr zpoisoned, Expr zmin, Expr zmax,
               std::string wvarname, Expr wpoisoned, Expr wmin, Expr wmax) {
    intervals = vec(Internal::VarInterval(xvarname, xpoisoned, xmin, xmax),
                    Internal::VarInterval(yvarname, ypoisoned, ymin, ymax),
                    Internal::VarInterval(zvarname, zpoisoned, zmin, zmax),
                    Internal::VarInterval(wvarname, wpoisoned, wmin, wmax));
}

Domain::Domain(std::string xvarname, bool xpoisoned, Expr xmin, Expr xmax) {
    intervals = vec(Internal::VarInterval(xvarname, Internal::make_bool(xpoisoned), xmin, xmax));
}

Domain::Domain(std::string xvarname, bool xpoisoned, Expr xmin, Expr xmax,
               std::string yvarname, bool ypoisoned, Expr ymin, Expr ymax) {
    intervals = vec(Internal::VarInterval(xvarname, Internal::make_bool(xpoisoned), xmin, xmax),
                    Internal::VarInterval(yvarname, Internal::make_bool(ypoisoned), ymin, ymax));
}

Domain::Domain(std::string xvarname, bool xpoisoned, Expr xmin, Expr xmax,
               std::string yvarname, bool ypoisoned, Expr ymin, Expr ymax,
               std::string zvarname, bool zpoisoned, Expr zmin, Expr zmax) {
    intervals = vec(Internal::VarInterval(xvarname, Internal::make_bool(xpoisoned), xmin, xmax),
                    Internal::VarInterval(yvarname, Internal::make_bool(ypoisoned), ymin, ymax),
                    Internal::VarInterval(zvarname, Internal::make_bool(zpoisoned), zmin, zmax));
}

Domain::Domain(std::string xvarname, bool xpoisoned, Expr xmin, Expr xmax,
               std::string yvarname, bool ypoisoned, Expr ymin, Expr ymax,
               std::string zvarname, bool zpoisoned, Expr zmin, Expr zmax,
               std::string wvarname, bool wpoisoned, Expr wmin, Expr wmax) {
    intervals = vec(Internal::VarInterval(xvarname, Internal::make_bool(xpoisoned), xmin, xmax),
                    Internal::VarInterval(yvarname, Internal::make_bool(ypoisoned), ymin, ymax),
                    Internal::VarInterval(zvarname, Internal::make_bool(zpoisoned), zmin, zmax),
                    Internal::VarInterval(wvarname, Internal::make_bool(wpoisoned), wmin, wmax));
}

/** Compute the intersection of two domains. */
Domain Domain::intersection(Domain other) {
    Domain result = *this; // Start with one of the domains as the 'answer'
    for (size_t i = 0; i < other.intervals.size() && i < intervals.size(); i++) {
        // Update corresponding dimensions from the other domain.
        Internal::VarInterval his = other.intervals[i];
        his.varname = result.intervals[i].varname; // Override variable name match. ***HACK***
        result.intervals[i].update(his);
    }
    return result;
}



namespace Internal {

int find(const std::vector<std::string> &varlist, std::string var) {
    for (size_t i = 0; i < varlist.size(); i++)
        if (varlist[i] == var)
            return i;
    return -1;
}



// BackwardIntervalInference walks an argument expression and
// determines the domain interval in the callee based on the
// domain interval in the caller, which is passed to it.

class BackwardIntervalInference : public IRVisitor {
public:
    Expr xmin;
    Expr xmax;
    std::string varname;
    Expr poison;
    const std::vector<std::string> &varlist;
    Domain &domain;
    
    BackwardIntervalInference(const std::vector<std::string> &v, Domain &dom, Expr axmin, Expr axmax, Expr xpoison) : 
        xmin(axmin), xmax(axmax), varname(""), poison(xpoison), varlist(v), domain(dom) {}
        
private:

    using IRVisitor::visit;
    
    // Not visited:
    // constants (IntImm, FloatImm) because they do not provide information about the
    // bounds of a variable except as they occur in known constructs (e.g. x + k).
    // Call, including abs, sin etc.
    //     abs is difficult because it can produce a domain broken into pieces.
    //     It would be used to mirror the domain.  A better result is to use a border
    //     handling function to reflect the borders.
    
    // When a node is visited and it turns out to be poison, we still need to visit the children
    // because we need to find out which variable has been poisoned.  In fact, multiple variables
    // could be poisoned.
    
    void visit(const Variable *op) {
        // Variable node defines the varname string - the variable for which we are
        // building the inverse function.
        int found = find(varlist, op->name);
        
        if (found < 0 || found >= (int) domain.intervals.size()) {
            // This is not a variable that we are interested in - it is probably a constant expression
            // arising from, for example, an ImageParam.
            // In the future, we should at least recognise some expressions and handle them.
            log(0) << "Warning: Domain inference skipping variable name " << op->name << ".\n";
            poison = const_true();
            return;
        }
        if (varname != "") {
            poison = const_true(); // Have already seen a variable in another branch
            if (varname != op->name) {
                // This is a different variable name than the one we are looking at primarily.
                // Mark that variable also as poison in the domain, although the data is not changed
                // because we are not touching the data that may already exist.
                domain.intervals[found].poison = const_true();
            }
            return; // Do not override the variable that we are studying.
        }
        varname = op->name;
    }
    
    void visit(const Add *op) {
        if (is_const(op->b)) {
            // Looking at constant on RHS of Add node.
            // e = x + k
            // x = e - k
            if (xmin.defined())
                xmin = xmin - op->b;
            if (xmax.defined())
                xmax = xmax - op->b;
            // Process the tree recursively
            op->a.accept(this);
        }
        else {
            assert(! is_const(op->a) && "Simplify did not put constant on RHS of Add");
            poison = const_true(); // Expression cannot be solved as it has branches not simplified out.
            op->a.accept(this);
            op->b.accept(this);
        }
    }
    
    void visit(const Sub *op) {
        // Simplify should convert x - 5 to x + -5.
        assert(! is_const(op->b) && "Simplify did not convert Sub of constant into negative Add");
        if (is_const(op->a)) {
            // e = k - x
            // x = k - e
            bool xmindef = xmin.defined();
            bool xmaxdef = xmax.defined();
            Expr new_xmin = xmax;
            if (xmaxdef)
                new_xmin = op->a - xmax;
            if (xmindef)
                xmax = op->a - xmin;
            if (xmaxdef)
                xmin = new_xmin;
            op->b.accept(this);
        }
        else {
            poison = const_true();
            op->a.accept(this);
            op->b.accept(this);
        }
    }
    
    void visit(const Mul *op) {
        assert(! is_const(op->a) && "Simplify did not put constant on RHS of Mul");
        if (is_const(op->b)) {
            // e = x * k
            // x = e / k
            // As a range, however, it is ceil(min/k) , floor(max/k)
            // We assume positive remainder semantics for division,
            // which is a valid assumption if the interval bounds are positive
            // Under these semantics, integer division always yields the floor.
            if (xmin.defined())
                xmin = (xmin + op->b - 1) / op->b;
            if (xmax.defined())
                xmax = xmax / op->b;
            op->a.accept(this);
        }
        else {
            poison = const_true();
            op->a.accept(this);
            op->b.accept(this);
        }
    }
    
    void visit(const Div *op) {
        if (is_const(op->b)) {
            // e = x / k
            // x = e * k
            // As a range, however, it is min * k to (max + 1) * k - 1
            // This is based on assumption of positive remainder semantics for
            // division and is intended to ensure that dividing by 2 produces a new
            // image that has every pixel repeated; i.e. the dimension is doubled.
            if (xmin.defined())
                xmin = xmin * op->b;
            if (xmax.defined())
                xmax = (xmax + 1) * op->b - 1;
            op->a.accept(this);
        }
        else {
            // e = k / x is not handled because it is not a linear transformation
            poison = const_true();
            op->a.accept(this);
            op->b.accept(this);
        }
    }

#if 0
    // Implementation of Mod is difficult: at the time when this
    // pass is run, xmin and/or xmax may be expressions.
    // A conservative result is to always use the intersection of
    // the range of e with the range 0 to k-1, but that gives an
    // interval that is too small in real situations.
    void visit(const Mod *op) {
        if (is_const(op->b)) {
            // e = x % k
            // If the range of e is 0 to k-1 or bigger then
            // the range of x is unconstrained.
            // If the range of e is smaller than 0 to k-1 then
            // the range of x is broken into pieces, of which the
            // canonical piece is the intersection of intervals 0 to k-1 and
            // xmin to xmax.
            poison = const_true();
        }
        else
            // e = k % x is not handled because it is not linear
            poison = const_true();
    }
    
    // Max
    // e = max(x,k) to be in range(a,b)
    // then x to be in range(c,b) where
    // if a <= k (i.e. max applied to x enforces the limit a effectively) then c = -infinity
    // else (i.e. max applied to x does not enforce the limit a) then c = a.
    
    // Min: analogous to Max.
#endif
};

VarInterval backwards_interval(const std::vector<std::string> &varlist, Domain &dom, Expr e, Expr xmin, Expr xmax, Expr xpoison) {
    log(3,"DI") << "e: " << e << "    min: " << xmin << "    max: " << xmax << '\n';
    BackwardIntervalInference infers(varlist, dom, xmin, xmax, xpoison);
    Expr e1 = simplify(e);
    
    e1.accept(&infers);
    
    VarInterval result(infers.varname, infers.poison, infers.xmin, infers.xmax);
    if (result.imin.defined()) result.imin = simplify(result.imin);
    if (result.imax.defined()) result.imax = simplify(result.imax);
    if (result.poison.defined()) {
        result.poison = result.poison || Internal::make_bool(infers.defaulted);
        result.poison = simplify(result.poison);
    }
    else
        result.poison = Internal::make_bool(infers.defaulted);
    
    // If it is known that the expression is inexact, then the range is set to infinite.
    // This is because whatever information was computed is incomplete and could be incorrect
    // This wont work if poison is an expression that is not constant: it becomes true always
    // because we have discarded the partial information that would apply when poison was not
    // true.  We could capture the detail by expressions of the form 
    // select(poison-expression,infinity,partial-information)
    // This is different than the case when separate expressions are analysed and one gives
    // concrete data and the other is inexact; in this case the result is inexact but
    // is restricted to the concrete data.  Of course, if we wanted to use expressions then
    // we could represent all the cases.
    log(2,"DI") << "Result: " << result.varname << "  " << result.poison << "  " << result.imin << "  " << result.imax << "\n";
    if (result.poison.defined() && ! equal(result.poison, const_false())) {
        result.imin = Expr();
        result.imax = Expr();
        if (! equal(result.poison, const_true())) {
            log(0) << "Warning: Poison was an expression; lost information.\n";
            result.poison = const_true();
        }
    }

    return result;
}


VarInterval backwards_interval(const std::vector<std::string> &varlist, Domain &dom, Expr e, Expr xmin, Expr xmax) {
    return backwards_interval(varlist, dom, e, xmin, xmax, const_false());
}


VarInterval backwards_interval(const std::vector<std::string> &varlist, Domain &dom, Expr e, VarInterval callee) {
    return backwards_interval(varlist, dom, e, callee.imin, callee.imax, callee.poison);
}





// ForwardDomainInference walks the parse tree inferring forward domain bounds
// for functions.
// This operates on the very raw parse tree before functions are realized etc
// so that we can interpret the index expressions.

// Class ForwardDomainInference is only for internal use in this module.
class ForwardDomainInference : public IRVisitor {
public:
    Domain dom; // The domain that we are building.
    Domain::DomainType dtype;
    const std::vector<std::string> &varlist;
    
    ForwardDomainInference(Domain::DomainType dt, const std::vector<std::string> &variables) : dtype(dt), varlist(variables) {
        for (size_t i = 0; i < variables.size(); i++)
            dom.intervals.push_back(VarInterval(variables[i], Internal::make_bool(false), Expr(), Expr()));
    }

private:

    using IRVisitor::visit;
    
    // The important methods are the visit methods that define handling of 
    // different tree nodes.
    
    // Call to a function, image buffer or image parameter.
    // e.g. g(x,y) = f(x,y-1);
    // From the expression y-1 we determine that the y bounds of g are
    // shifted compared to the y bounds of f.
    void visit (const Call *func_call)
    {
        // Arguments are in std::vector<Expr> func_call->args
        // If it is a call to another Halide function, Function func_call->func points to it
        // If it is a direct reference to an image, Buffer func_call->image is the buffer
        // and if it is an image parameter, Parameter func_call->param is that.
        // To check use ->func.value().defined(), ->image.defined() and ->param.defined().
        
        //log(0,"LH") << "Call: " << func_call->name;
        // Look at the call type 
        //if (func_call->call_type == Call::Image)
            //log(0) << " image";
        //else if (func_call->call_type == Call::Extern)
            //log(0) << " extern";
        //else if (func_call->call_type == Call::Halide)
            //log(0) << " halide";
        //else
            //log(0) << " unknown";
        //log(0) << '\n';
        // Each of the argument expressions must be processed in turn.
        for (size_t i = 0; i < func_call->args.size(); i++)
        {
            // func_call->args[i] is the i'th dimension index expression for the call.
            // For now, just print it out.
            //log(0,"LH") << "arg " << i << ": " << func_call->args[i] << '\n';
            
            // Perform backward interval analysis on the argument expression.
            // Have to know the valid domain for the actual argument of the function.
            // For images, just use image dimensions.  All the different types of domain are the same
            // for images.
            VarInterval result;
            if (func_call->call_type == Call::Image)
            {
                if (func_call->image.defined()) {
                    result = backwards_interval(varlist, dom, func_call->args[i],
                                            func_call->image.min(i), 
                                            func_call->image.min(i) + func_call->image.extent(i) - 1);
                }
                else if (func_call->param.defined()) {
                    result = backwards_interval(varlist, dom, func_call->args[i],
                                            0, func_call->param.extent(i) - 1);
                }
                else
                    assert(0 && "Call to Image is neither image nor imageparam\n");
            }
            else if (func_call->call_type == Call::Halide)
            {
                // For Halide calls, access the domain in the function object.
                // We need to know the VarInterval of the i'th parameter of the called function.
                result = backwards_interval(varlist, dom, func_call->args[i],
                                        func_call->func.domain(dtype).intervals[i]);
            }
            
            // For known call types, log results and update the caller's domain.
            if (func_call->call_type == Call::Image || func_call->call_type == Call::Halide) {
                //log(0) << "arg: " << func_call->args[i] << "    ";
                //if (equal(result.poison, const_true()))
                    //log(0,"LH") << "poison\n";
                //else {
                    //log(0,"LH") << "imin: " << result.imin << "    ";
                    //log(0,"LH") << "imax: " << result.imax << "    ";
                    //log(0,"LH") << "v: " << result.varname;
                    //if (! equal(result.poison, const_false()))
                        //log(0,"LH") << "    poison: " << result.poison << '\n';
                    //else
                        //log(0,"LH") << '\n';
                //}
                        
                // Search through the variables in the Domain and update the appropriate one
                int index = find(varlist, result.varname);
                if (index >= 0) {
                    dom.intervals[index].update(result);
                }
                else if (! result.varname.empty()) { // Empty varname means no interval information.
                    log(0) << "Warning: The interval variable " << result.varname << " is unknown\n";
                }
                
            }
        }
    }
};



/* Notes
Difference between Var and Variable.  Variable is a parse tree node.
Var is just a name.
*/

Domain domain_inference(Domain::DomainType dtype, const std::vector<std::string> &variables, Expr e)
{
    // At this level, we use a list of variables passed in.
    ForwardDomainInference infers(dtype, variables);
    
    assert(e.defined() && "domain_inference applied to undefined expression");
    
    e.accept(&infers);
    
    return infers.dom;
}


void check_interval(std::vector<std::string> varlist, Expr e, Expr xmin, Expr xmax, 
                    bool correct_poison_bool, Expr correct_min, Expr correct_max, 
                    std::string correct_varname) {
    Expr correct_poison;
    Domain dom("x", false, Expr(), Expr(), "y", false, Expr(), Expr()); // A working domain for the test variables x and y
    correct_poison = Internal::make_bool(correct_poison_bool);
    VarInterval result = backwards_interval(varlist, dom, e, xmin, xmax);
    
    Expr e1 = simplify(e); // Duplicate simplification for debugging only
    log(1,"DI") << "e: " << e << "    ";
    log(2,"DI") << "e1: " << e1 << "    ";
    
    if (equal(result.poison, const_true()))
        log(1,"DI") << "inexact\n";
    else {
        log(1,"DI") << "imin: " << result.imin << "    ";
        log(1,"DI") << "imax: " << result.imax << "    ";
        log(1,"DI") << "v: " << result.varname;
        if (! equal(result.poison, const_false()))
            log(1,"DI") << "    inexact: " << result.poison << '\n';
        else
            log(1,"DI") << '\n';
    }
    
    bool success = true;
    if (! equal(result.poison, correct_poison)) {
        std::cout << "Incorrect poison: " << result.poison << "    "
                  << "Should have been: " << correct_poison << std::endl;
        success = false;
    }
    if (! correct_poison_bool) {
        // Only bother to check the details if it is not supposed to be poison
        if (!equal(result.imin, correct_min)) {
            std::cout << "Incorrect imin: " << result.imin << "    "
                      << "Should have been: " << correct_min << std::endl;
            success = false;
        }
        if (!equal(result.imax, correct_max)) {
            std::cout << "Incorrect imax: " << result.imax << "    "
                      << "Should have been: " << correct_max << std::endl;
            success = false;
        }
        if (result.varname != correct_varname) {
            std::cout << "Incorrect variable name: " << result.varname << "    "
                      << "Should have been: " << correct_varname << std::endl;
            success = false;
        }
    }
    assert(success && "Domain inference test failed");
}

void backward_interval_test() {
    Var x("x"), y("y");
    std::string v = "<dummy>";
    Expr cmin, cmax;
    std::vector<std::string> varlist = vecS("x","y","z","w");
    
    // Tests of backward interval inference
    check_interval(varlist, x, 0, 100, false, 0, 100, "x");
    check_interval(varlist, x + 1, 0, 100, false, -1, 99, "x");
    check_interval(varlist, 1 + x, 0, 100, false, -1, 99, "x");
    check_interval(varlist, 1 + x + 1, 0, 100, false, -2, 98, "x");
    check_interval(varlist, x - 1, 0, 100, false, 1, 101, "x");
    check_interval(varlist, 1 - x, 0, 100, false, -99, 1, "x");
    //check_interval(varlist, x + x, 0, 100, cmin, cmax, v);
    // Tests that use * and / should ensure that results are positive
    // so that positive remainder semantics hold for division
    // (until these semantics are actually implemented in Halide)
    check_interval(varlist, 2 * x, 10, 100, false, 5, 50, "x");
    check_interval(varlist, x * 2, 10, 100, false, 5, 50, "x");
    check_interval(varlist, x / 2, 10, 100, false, 20, 201, "x");
    // x = 19  e = (19 + 1) / 2 = 10
    // x = 200  e = (201) / 2 = 100
    check_interval(varlist, (x + 1) / 2, 10, 100, false, 19, 200, "x");
    check_interval(varlist, (x + 2) / 2, 10, 100, false, 18, 199, "x");
    // (2 * x + 4) / 2 is the same as x + 2
    check_interval(varlist, (2 * x + 4) / 2, 10, 100, false, 8, 98, "x");
    // x = 8  e = (16 + 5) / 2 = 10
    // x = 98  e = (196 + 5) / 2 = 201 / 2 = 100
    // This expression also simplifies to x + 2
    check_interval(varlist, (2 * x + 5) / 2, 10, 100, false, 8, 98, "x");
    // x = 5  e = (15 + 5) / 2 = 10
    // x = 65  e = (195 + 5) / 2 = 100   but x = 66 is too big
    check_interval(varlist, (3 * x + 5) / 2, 10, 100, false, 5, 65, "x");
    // x = 7  e = (21 + 5) / 2 - 2 = 11  but x=6 e=(18+5)/2-2 = 9
    // x = 66  e = (198 + 5) / 2 - 2 = 99   but x=67 e=(201+5)/2-2=101
    check_interval(varlist, (3 * x + 5) / 2 - 2, 10, 100, false, 7, 66, "x");
    
    // Constant expressions are poison. They provide no constraint on the caller's
    // variables, although they may result in out-of-bounds errors on the callee.
    // But checking for out-of-bounds errors is a separate task.
    check_interval(varlist, Expr(5) + 7, 0, 100, true, Expr(), Expr(), "");
    check_interval(varlist, 105, 0, 100, true, Expr(), Expr(), "");
    // Expression is poison because it contains a node type that 
    // is not explicitly handled
    check_interval(varlist, sin(x), 10, 100, true, 0, 0, "");
    // Expression is poison because it contains more than one variable
    // Actually, it is detected as poison by Add node because it is not x + k
    check_interval(varlist, x + y, 0, 100, true, 0, 0, "");
    return;
}




void check_domain_expr(Domain::DomainType dtype, std::vector<std::string> variables, Expr e, Domain d) {
    // Compute the domain of the expression using forward domain inference.
    Domain edom = domain_inference(dtype, variables, e);
    
    log(1,"DI") << "e: " << e << '\n';
    for (size_t i = 0; i < edom.intervals.size(); i++)
        log(1,"DI") << "    " << edom.intervals[i].varname 
                    << ": imin: " << edom.intervals[i].imin 
                    << "  imax: " << edom.intervals[i].imax 
                    << "  poison: " << edom.intervals[i].poison << '\n';
    
    // Compare the computed domain with the expected domain
    bool success = true;
    if (d.intervals.size() != edom.intervals.size()) {
        std::cout << "Incorrect domain size: " << edom.intervals.size()
                  << "    Should have been: " << d.intervals.size() << '\n';
        success = false;
    }
    for (size_t i = 0; i < edom.intervals.size(); i++) {
        if (edom.intervals[i].varname != variables[i]) {
            std::cout << "Incorrect variable name: " << edom.intervals[i].varname
                      << "    Should have been: " << variables[i] << '\n';
            success = false;
        }
        if (edom.intervals[i].varname != d.intervals[i].varname) {
            std::cout << "Incorrect variable name: " << edom.intervals[i].varname
                      << "    Template answer: " << d.intervals[i].varname << '\n';
            success = false;
        }
        if (! equal(edom.intervals[i].poison, d.intervals[i].poison)) {
            std::cout << "Incorrect poison: " << edom.intervals[i].poison
                      << "    Should have been: " << d.intervals[i].poison << '\n';
            success = false;
        }
        // Check the numeric range even if it is poison
        if (! equal(edom.intervals[i].imin, d.intervals[i].imin)) {
            std::cout << "Incorrect imin: " << edom.intervals[i].imin
                      << "    Should have been: " << d.intervals[i].imin << '\n';
            success = false;
        }
        if (! equal(edom.intervals[i].imax, d.intervals[i].imax)) {
            std::cout << "Incorrect imax: " << edom.intervals[i].imax
                      << "    Should have been: " << d.intervals[i].imax << '\n';
            success = false;
        }
    }
    assert(success && "Domain inference test failed");
}

void domain_expr_test()
{
    Image<uint8_t> in(20,40);
    Image<uint8_t> inb(30,35);
    Func f("f"), g("g"), h("h"), fa("fa"), fb("fb"), fc("fc");
    Var x("x"), y("y"), a("a"), b("b"), ext("fff.extent.0");
    Expr False = Internal::make_bool(false);
    Expr True = Internal::make_bool(true);
    
    check_domain_expr(Domain::Valid, vecS("iv.0", "iv.1"), in, Domain("iv.0", False, 0, 19, "iv.1", False, 0, 39));
    check_domain_expr(Domain::Valid, vecS("x", "y"), in(x-2,y), Domain("x", False, 2, 21, "y", False, 0, 39));
    check_domain_expr(Domain::Valid, vecS("x", "y"), in(x-2,y) + in(x,y), Domain("x", False, 2, 19, "y", False, 0, 39));
    check_domain_expr(Domain::Valid, vecS("x", "y"), in(x-2,y) + in(x,y) + in(x,y+5), 
                        Domain("x", False, 2, 19, "y", False, 0, 34));
    // Inexact results due to use of min function. The expression y is the only understood expression.
    check_domain_expr(Domain::Valid, vecS("x", "y"), in(x-2,y) + in(x,y) + in(x,min(y+5,15)), 
                        Domain("x", False, 2, 19, "y", True, 0, 39));
    // Inexact results due to use of max function.  
    check_domain_expr(Domain::Valid, vecS("x", "y"), in(x-2,max(y,1)) + in(max(x,0),y) + in(min(x,9),y+5), 
                        Domain("x", True, 2, 21, "y", True, 0, 34));
    // Test interchange of variables (flip the domain of the function)
    check_domain_expr(Domain::Valid, vecS("x", "y"), in(y,x), 
                        Domain("x", False, 0, 39, "y", False, 0, 19));
    // Test multiple use of the same variable
    check_domain_expr(Domain::Valid, vecS("x"), in(x,x), 
                        Domain("x", False, 0, 19));
    // Test mixture of variables
    check_domain_expr(Domain::Valid, vecS("x", "y"), in(x+y,y), 
                        Domain("x", True, Expr(), Expr(), "y", True, 0, 39));
    // Test domain of a constant expression
    check_domain_expr(Domain::Valid, vecS("x", "y"), 3, 
                        Domain("x", False, Expr(), Expr(), "y", False, Expr(), Expr()));
    // Test domain with a constant variable - treat it as though it were not a variable
    check_domain_expr(Domain::Valid, vecS("x", "y"), ext, 
                        Domain("x", False, Expr(), Expr(), "y", False, Expr(), Expr()));
    // Test domain with a constant variable - treat it as though it were not a variable
    // This test should produce a domain expression that involves fff.extent.0 but that is not yet implemented.
    check_domain_expr(Domain::Valid, vecS("x", "y"), in(x - ext, y), 
                        Domain("x", True, Expr(), Expr(), "y", False, 0, 39)); // **** TO BE FIXED  "x", False, ext, ext + 39....

    f(x,y) = in(x-1,y) - in(x,y);
    check_domain_expr(Domain::Valid, vecS("x","y"), f(x,y-1), Domain("x", False, 1, 19, "y", False, 1, 40));
    
    // Function g is in with border replication simulated by clamp with manual setting of computable and valid regions
    g(x,y) = in(clamp(x,0,in.width()-1), clamp(y,0,in.height()-1)); // Old way to clamp
	// The notation g.infinite() is untidy - it would seem that we should use Domain::infinite() instead, but
    // the problem is that the number of dimensions is unknown.  One option is to assume that a complete lack
    // of information in a dimension means infinite, in which case the empty Domain is also an finite Domain.
    g.computable() = g.infinite(); // Set the computable region first (in case we intersect valid with it)
    g.valid() = in.valid(); // Manually update the valid region.
    
    // h is a kernel function of g, using the border handling
    h(a,b) = g(a,b)+g(a,b-1)+g(a,b+1);
    h.kernel(g); // h is a kernel function of g, so the valid region is copied and intersected with computable region
    check_domain_expr(Domain::Valid, vecS("x","y"), h(x,y), Domain("x", False, 0, 19, "y", False, 0, 39));
    check_domain_expr(Domain::Computable, vecS("x","y"), h(x,y), Domain("x", False, Expr(), Expr(), "y", False, Expr(), Expr()));

    // fa is a kernel function of both g and inb
    // Note that the computable domain of inb limits the computable domain of fa
    fa(x,b) = g(x-1,b+1) + inb(x,b-2);
    // Before declaring fa to be kernel function, the automatically derived domains should be as follows.
    // The shifts of the valid domains have impacted the final valid domain, and the computable domain is 
    // also influenced by shifting of inb.
    check_domain_expr(Domain::Valid, vecS("x","y"), fa(x,y), Domain("x", False, 1, 20, "y", False, 2, 36));
    check_domain_expr(Domain::Computable, vecS("x","y"), fa(x,y), Domain("x", False, 0, 29, "y", False, 2, 36));
    fa.kernel(g,inb);
    // Declaring fa to be a kernel function overrides the shifts implicit in the definition of fa.
    // For g, this means that the valid domain is copied but for inb the computable domain is restricting the valid
    // domain.
    check_domain_expr(Domain::Valid, vecS("x","y"), fa(x,y), Domain("x", False, 0, 19, "y", False, 2, 34));
    check_domain_expr(Domain::Computable, vecS("x","y"), fa(x,y), Domain("x", False, 0, 29, "y", False, 2, 36));
    return;
}

void domain_inference_test()
{
    backward_interval_test();
    domain_expr_test();
    std::cout << "Domain inference test passed" << std::endl;
}

}
}