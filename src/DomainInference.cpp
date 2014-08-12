#include "IR.h"
#include "Var.h"
#include "Func.h"
#include "Image.h"
#include "IREquality.h"
#include "IROperator.h"
#include "IRVisitor.h"
#include "IRPrinter.h"
#include "Log.h"
#include "Simplify.h"
#include "Solver.h"
#include "InlineLet.h"
#include "DomInterval.h"
#include "CodeLogger.h"

#include <iostream>

using std::string;
using std::ostream;

namespace Halide { 
// Domain: Represents a domain whether valid, defined or some other.
Domain::Domain() {
    intervals.clear();
    domain_locked = false;
}

Domain::Domain(Expr xmin, Expr xmax) {
    intervals = Internal::vec(DomInterval(Int(32), xmin, xmax, true));
    domain_locked = false;
}

Domain::Domain(Expr xmin, Expr xmax, Expr ymin, Expr ymax) {
    intervals = Internal::vec(DomInterval(Int(32), xmin, xmax, true),
                              DomInterval(Int(32), ymin, ymax, true));
    domain_locked = false;
}

Domain::Domain(Expr xmin, Expr xmax, Expr ymin, Expr ymax, Expr zmin, Expr zmax) {
    intervals = Internal::vec(DomInterval(Int(32), xmin, xmax, true),
                              DomInterval(Int(32), ymin, ymax, true),
                              DomInterval(Int(32), zmin, zmax, true));
    domain_locked = false;
}

Domain::Domain(Expr xmin, Expr xmax, Expr ymin, Expr ymax, Expr zmin, Expr zmax, Expr wmin, Expr wmax) {
    intervals = Internal::vec(DomInterval(Int(32), xmin, xmax, true),
                              DomInterval(Int(32), ymin, ymax, true),
                              DomInterval(Int(32), zmin, zmax, true),
                              DomInterval(Int(32), wmin, wmax, true));
    domain_locked = false;
}

Domain::Domain(DomInterval xint) {
    intervals = Internal::vec(DomInterval(xint));
    domain_locked = false;
}

Domain::Domain(DomInterval xint, DomInterval yint) {
    intervals = Internal::vec(DomInterval(xint), 
                              DomInterval(yint));
    domain_locked = false;
}

Domain::Domain(DomInterval xint, DomInterval yint,
               DomInterval zint) {
    intervals = Internal::vec(DomInterval(xint), 
                              DomInterval(yint),
                              DomInterval(zint));
    domain_locked = false;
}

Domain::Domain(DomInterval xint, DomInterval yint,
               DomInterval zint, DomInterval wint) {
    intervals = Internal::vec(DomInterval(xint), 
                              DomInterval(yint),
                              DomInterval(zint),
                              DomInterval(wint));
    domain_locked = false;
}

Domain Domain::infinite(int dimensions) {
    Expr neginf = Internal::make_infinity(Int(32), -1);
    Expr posinf = Internal::make_infinity(Int(32), +1);
    switch (dimensions) {
    case 0:
        return Domain();
    case 1:
        return Domain(neginf, posinf);
    case 2:
        return Domain(neginf, posinf, neginf, posinf);
    case 3:
        return Domain(neginf, posinf, neginf, posinf, neginf, posinf);
    case 4:
        return Domain(neginf, posinf, neginf, posinf, neginf, posinf,
                      neginf, posinf);
    }
    std::cerr << "Cannot construct domain with dimensions=" << dimensions << "\n";
    assert(0);
    return Domain();
}

/** Compute the intersection of two domains. */
Domain Domain::intersection(const Domain other) const {
    Domain result = *this; // Start with one of the domains as the 'answer'
    assert(other.intervals.size() == intervals.size() && "Intersection of domains - must have the same dimensionality");
    for (size_t i = 0; i < other.intervals.size() && i < intervals.size(); i++) {
        // Update corresponding dimensions from the other domain.
        result.intervals[i] = Halide::intersection(result.intervals[i], other.intervals[i]);
    }
    return result;
}

const Expr Domain::min(int index) const {
    assert(index >= 0 && index < (int) intervals.size() && "Attempt to access Domain out of range");
    return intervals[index].min;
}

const Expr Domain::max(int index) const {
    assert(index >= 0 && index < (int) intervals.size() && "Attempt to access Domain out of range");
    return intervals[index].max;
}

const bool Domain::exact(int index) const {
    assert(index >= 0 && index < (int) intervals.size() && "Attempt to access Domain out of range");
    return intervals[index].exact;
} 


const Expr Domain::extent(int index) const {
    assert(index >= 0 && index < (int) intervals.size() && "Attempt to access Domain out of range");
    // Use the same expression form as in IntRange converting from IntInterval to Range
    return Internal::simplify((intervals[index].max + 1) - intervals[index].min);
}

const int Domain::imin(int index) const {
    int ival;
    assert(get_const_int(Domain::min(index), ival) && "Domain minimum value is not integer constant");
    return ival;
}

const int Domain::imax(int index) const {
    int ival;
    assert(get_const_int(Domain::max(index), ival) && "Domain maximum value is not integer constant");
    return ival;
}

const int Domain::iextent(int index) const {
    int ival;
    bool status = get_const_int(Domain::extent(index), ival);
    if (! status) {
        std::cerr << "Domain extent for index " << index << " is not an integer constant" << std::endl;
        std::cerr << "Domain is: " << *this << std::endl;
        assert(false && "Domain extent value is not an integer constant");
    }
    return ival;
}


namespace Internal {

int find(const std::vector<std::string> &varlist, std::string var) {
    for (size_t i = 0; i < varlist.size(); i++)
        if (varlist[i] == var)
            return i;
    return -1;
}

# if 0
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
        // Check whether variable name is in the list of known names
        result |= (find(varlist, op->name) >= 0);
    }
};

// is_constant_expr: Determine whether an expression is constant relative to a list of free variables
// that may not occur in the expression.
static bool is_constant_expr(std::vector<std::string> varlist, Expr e) {
    // Walk the expression; if no variables in varlist are found then it is a constant expression
    HasVariable hasvar(varlist);
    e.accept(&hasvar);
    return ! hasvar.result;
}
# endif

// BackwardIntervalInference walks an argument expression and
// determines the domain interval of a variable in the caller based on the
// domain interval of the expression in the callee, which is passed to it.

# define BACK_LOGLEVEL 4

class BackwardIntervalInference : public IRVisitor {
public:
    std::vector<DomInterval> callee; // Intervals from the callee, updated to infer intervals for variable
    std::string varname;
    const std::vector<std::string> &varlist;
    std::vector<Domain> &domains; // Domains of the caller, updated directly as required (inexact result)
    
    BackwardIntervalInference(const std::vector<std::string> &_varlist, std::vector<Domain> &_domains, std::vector<DomInterval> _callee) : 
        callee(_callee), varname(""), varlist(_varlist), domains(_domains) {
        assert(_callee.size() == Domain::MaxDomains && "Incorrect number of callee intervals provided");
    }
        
private:

    using IRVisitor::visit;
    
    bool is_constant_expr(Expr e) {
        return Internal::is_constant_expr(varlist, e);
    }

    void set_callee_exact_false() {
        for (int j = Domain::Valid; j < Domain::MaxDomains; j++) {
            callee[j].exact = false;
        }
    }
    
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
        
        if (found < 0 || found >= (int) varlist.size()) {
            // This is not a variable that we are interested in - it is probably a constant expression
            // arising from, for example, an ImageParam.
            // In the future, we should at least recognise some expressions and handle them.
            log(0) << "Warning: Domain inference skipping variable name " << op->name << ".\n";
            // We cannot exactly determine the interval from such an unknown variable,
            // although we often would not need to.
            set_callee_exact_false();
            return;
        }
        if (varname != "") {
            log(BACK_LOGLEVEL,"DOMINF") << "Duplicate variable " << op->name << "  (original " << varname << ")\n";
            set_callee_exact_false(); // Have already seen a variable in another branch.  It is now not exact
            if (varname != op->name) {
                // This is a different variable name than the one we are looking at primarily.
                // Mark that variable also as inexact in the domain, although the data is not changed
                // because we are not touching the data that may already exist.
                for (unsigned int j = Domain::Valid; j < domains.size(); j++) {
                    domains[j].intervals[found].exact = false;
                }
            }
            return; // Do not override the variable that we are studying.
        }
        varname = op->name;
        log(BACK_LOGLEVEL,"DOMINF") << "Observe variable " << op->name << "\n";
    }
    
    void visit(const Add *op) {
        log(4,"DOMINF") << "Add\n";
        if (is_constant_expr(op->b)) {
            // Looking at constant on RHS of Add node.
            // e = x + k
            // x = e - k
            log(4,"DOMINF") << "Add constant\n";
            for (int j = Domain::Valid; j < Domain::MaxDomains; j++) {
                log(4,"DOMINF") << "Before [" << j << "]: " << callee[j] << "\n";
                callee[j] = inverseAdd(callee[j], op->b);
                log(4,"DOMINF") << "After  [" << j << "]: " << callee[j] << "\n";
            }
            // Process the tree recursively
            op->a.accept(this);
        }
        else if (is_constant_expr(op->a)) {
            // Looking at constant on LHS of Add node.
            // e = k + x ==> e1 = x + k
            Expr e = op->b + op->a;
            e.accept(this);
        }
        else {
            set_callee_exact_false(); // Expression cannot be solved as it has branches not simplified out.
            // Still must process the childen recursively in order to identify variables marked not exact
            IRVisitor::visit(op);
        }
    }
    
    void visit(const Sub *op) {
        // Simplify should convert x - 5 to x + -5.
        assert(! is_const(op->b) && "Simplify did not convert Sub of constant into negative Add");
        if (is_constant_expr(op->a)) {
            // e = k - x
            // x = k - e
            for (int j = Domain::Valid; j < Domain::MaxDomains; j++) {
                callee[j] = inverseSubA(op->a, callee[j]);
            }
            op->b.accept(this);
        }
        else if (is_constant_expr(op->b)) {
            // e = x - k
            // x = e + k
            for (int j = Domain::Valid; j < Domain::MaxDomains; j++) {
                callee[j] = inverseSub(callee[j], op->b);
            }
            op->a.accept(this);
        }
        else {
            set_callee_exact_false();
            IRVisitor::visit(op);
        }
    }
    
    void visit(const Mul *op) {
        assert(! is_const(op->a) && "Simplify did not put constant on RHS of Mul");
        if (is_constant_expr(op->b)) {
            // e = x * k
            // x = e / k
            // As a range, however, it is ceil(min/k) , floor(max/k)
            for (int j = Domain::Valid; j < Domain::MaxDomains; j++) {
                callee[j] = inverseMul(callee[j], op->b);
            }
            op->a.accept(this);
        }
        else if (is_constant_expr(op->a)) {
            // e = k * x ==>  e1 = x * k
            Expr e = op->b * op->a;
            e.accept(this);
        }
        else {
            set_callee_exact_false();
            IRVisitor::visit(op);
        }
    }
    
    void visit(const Div *op) {
        if (is_constant_expr(op->b)) {
            // e = x / k
            // x = e * k
            // As a range, however, it is min * k to (max + 1) * k - 1
            for (int j = Domain::Valid; j < Domain::MaxDomains; j++) {
                callee[j] = zoom(callee[j], op->b);
            }
            op->a.accept(this);
        }
        else {
            // e = k / x is not handled because it is not a linear transformation
            set_callee_exact_false();
            IRVisitor::visit(op);
        }
    }

    // How to handle Mod if it is NOT treated as border handling
    // To handle it as border handling, call clamp_limits.
    void visit(const Mod *op) {
        for (int j = Domain::Valid; j < Domain::MaxDomains; j++) {
            log(3,"DOMINF") << "Mod(" << op->a << ",  " << op->b << ") on " << callee[j] << "\n";
        }
        if (is_constant_expr(op->b)) {
            // e = x % k  (for positive k)
            // If the range of e is 0 to k-1 or bigger then
            // the range of x is unconstrained.
            // If the range of e does not fully cover 0 to k-1 then
            // the range of x is broken into pieces, of which the
            // canonical piece is the intersection of intervals 0 to k-1 and
            // xmin to xmax.
            // Given the definition of Mod, it may be possible to determine
            // a better canonical range for x; however, it would still be an inexact representation.
            for (int j = Domain::Valid; j < Domain::MaxDomains; j++) {
                callee[j] = inverseMod(callee[j], op->b);
            }
            op->a.accept(this);
        }
        else {
            // e = k % x is not handled because it is not linear
            set_callee_exact_false();
            IRVisitor::visit(op);
        }
    }
    
    /** Domain inference on border handlers.
     * Consider a call f(clamp(x)) where clamp is some clamp-like border handling index expression.
     * The question is: Given the domains of f, what are the domains of x?
     * 
     * Effective border handler.  A border handler is considered EFFECTIVE if the clamp limits
     * are contained in the valid domain of f.  This means that the border handler prevents access
     * to pixels outside the valid domain of f.
     * Computable domain: If the border handler is effective, then the computable domain applicable to
     *    x is infinite.  If the border handler is not effective, then the computable domain is restricted
     *    to the VALID domain of f intersected with the clamp interval because the semantics of applying
     *    a border handler is that no access (not even a padded access) can exceed the valid domain
     *    of the underlying image/function.
     * Valid domain: If the border handler is effective, then the valid domain is the clamp interval
     *    because data outside that interval is border handled.  If the border handler is not effective,
     *    then the valid domain is the intersection of valid domain of f with clamp interval.
     * Partially effective border handler: One clamp limit is within the valid domain of f, the other is not.
     *    General border handlers cannot be partially effective. Halide's clamp operator, i.e. Min and Max,
     *    can be partially effective because when the clamp is applied, the value is moved to the clamp
     *    limit.  In this case, if a clamp limit is within the valid domain of f, then that 
     *    limit is partially effective in its own right: everything above/below that limit is
     *    clamped to a value inside the valid domain of f.  The corresponding limit of x is infinite.
     *    An operation like mod (or Border::wrap) cannot be partially effective because a value of x
     *    just outside one limit is wrapped to the other limit; for the wrapped value to be in range,
     *    both limits must be in range.
     *
     * Parameters
     * v: The domain intervals of the enclosing clamp expression.
     * t: The Halide type of the operand of the clamp expression - used to create infinities.
     * op_min, op_max: The minimum and maximum limit expressions.  In the case that
     *    the operator has only one limit expression, the other MUST be undefined to indicate
     *    that it is not even trying to impose a limit.
     * partially_effective: True if the clamp operation can be partially effective.
     * return value: The domain intervals applicable to the enclosing expression.
     */
    std::vector<DomInterval> solve_clamp_limits(std::vector<DomInterval> v, Type t, Expr op_min, Expr op_max, bool partially_effective) {
        // Start with a copy of the input domain intervals
        std::vector<DomInterval> result(v);
        bool min_def = op_min.defined();
        bool max_def = op_max.defined();
        // Expressions representing whether the limits are effective or not.
        Expr e_effective_min, e_effective_max;
        if (min_def) e_effective_min = op_min >= v[Domain::Valid].min;
        if (max_def) e_effective_max = op_max <= v[Domain::Valid].max;
        if (! partially_effective && min_def && max_def) {
            // Clamp-like operator that cannot be partially effective.
            // Must be effective at both ends, or neither is considered effective
            e_effective_min = e_effective_max = (e_effective_min && e_effective_max);
        }
        // If the border handler is effective at a particular end then the
        // computable domain at that end becomes infinity and the valid domain limit
        // becomes the clamp limit.
        // If the border handler is not effective at the end, then both the computable and
        // the valid domains are limited to the tighter of the clamp limit or the
        // incoming valid domain.
        // In fact, whether or not the border handler is effective, the valid domain
        // is limited to the tighter of the clamp limit and the incoming valid domain
        // because when it is effective it is the clamp limit that is tighter.
        // However, if op_min or op_max is undefined (i.e. no limit being applied)
        // then the corresponding bounds of the domain intervals are unchanged.
        if (min_def) {
            result[Domain::Computable].min = simplify(select(e_effective_min, make_infinity(t, -1),
                                                             max(op_min, v[Domain::Valid].min)));
            result[Domain::Valid].min = simplify(max(op_min, v[Domain::Valid].min));
        }
        if (max_def) {
            result[Domain::Computable].max = simplify(select(e_effective_max, make_infinity(t, +1),
                                                             min(op_max, v[Domain::Valid].max)));
            result[Domain::Valid].max = simplify(min(op_max, v[Domain::Valid].max));
        }
        return result;
    }

    
    // Method to handle clamp-like operations, which are treated as border handlers.
    // Clamp(op_a, op_min, op_max)
    void clamp_limits(Expr op_a, Expr op_min, Expr op_max, bool partially_effective) {
        log(3,"DOMINF") << "C: " << callee[Domain::Computable] << "  V: " << callee[Domain::Valid] << "\n";
        callee = solve_clamp_limits(callee, op_a.type(), op_min, op_max, partially_effective);
        log(3,"DOMINF") << "C: " << callee[Domain::Computable] << "  V: " << callee[Domain::Valid] << "\n";
        op_a.accept(this);
    }
    
    void visit(const Clamp *op) {
        log(2,"DOMINF") << Expr(op) << "\n";
        for (int j = Domain::Valid; j < Domain::MaxDomains; j++) {
            log(3,"DOMINF") << Expr(op) << " on " << callee[j] << "\n";
        }
        // Clamp operators are particularly significant for forward domain inference.
        if (op->clamptype == Clamp::None) {
            // Clamp::None indicates that the valid domain of the callee should be
            // used as the computable domain - i.e. disallow access outside the valid domain.
            // So, simply put, whatever Valid domain we have inferred for the Clamp::None(e(x)) expression,
            // that also becomes the computable domain for e(x).  Of course, e(x) may then
            // apply border handling although such as strategy is rare.
            callee[Domain::Computable] = callee[Domain::Valid];
            op->a.accept(this);
        }
        else {
            clamp_limits(op->a, op->min, op->max, op->clamptype == Clamp::Replicate); // replicate can be partially effective.
        }
    }

    // Max as border handler
    void visit(const Max *op) {
        log(2,"DOMINF") << Expr(op) << "\n";
        for (int j = Domain::Valid; j < Domain::MaxDomains; j++) {
            log(3,"DOMINF") << "Max(" << op->a << ",  " << op->b << ") on " << callee[j] << "\n";
        }
        if (is_constant_expr(op->b)) {
            // max(a,b) is equivalent to clamping on the range (b,infinity)
            clamp_limits(op->a, op->b, Expr(), true);  // Can be partially effective 
        }
        else if (is_constant_expr(op->a)) {
            // Interchange the parameters
            clamp_limits(op->b, op->a, Expr(), true);
        }
        else {
            // max of two non-constant expressions will lead to inexact results
            // because it is not linear
            // Note: if the expressions were max(x+k1, x+k2) or similar then
            // simplification could reduce the expression if it could prove one of them bigger.
            set_callee_exact_false();
            IRVisitor::visit(op);
        }
    }
    
    // Min as border handler
    void visit(const Min *op) {
        log(2,"DOMINF") << Expr(op) << "\n";
        for (int j = Domain::Valid; j < Domain::MaxDomains; j++) {
            log(3,"DOMINF") << "Min(" << op->a << ",  " << op->b << ") on " << callee[j] << "\n";
        }
        if (is_constant_expr(op->b)) {
            clamp_limits(op->a, Expr(), op->b, true); // Can be partially effective
        }
        else if (is_constant_expr(op->a)) {
            // Interchange the parameters
            clamp_limits(op->b, Expr(), op->a, true);
        }
        else {
            // min of two non-constant expressions will lead to inexact results
            // because it is not linear
            set_callee_exact_false();
            IRVisitor::visit(op);
        }
    }
    
};


std::vector<DomInterval> backwards_interval(const std::vector<std::string> &varlist, std::vector<Domain> &domains, Expr e, 
                                            std::vector<DomInterval> callee, std::string* varname = NULL) {
    assert(callee.size() == Domain::MaxDomains && "Incorrect number of callee intervals");
    for (int j = Domain::Valid; j < Domain::MaxDomains; j++) {
        log(BACK_LOGLEVEL,"DOMINF") << "e: " << e << "  [" << j << "]: " << callee[j] << '\n';
    }
    
    BackwardIntervalInference infers(varlist, domains, callee);
    Expr e1 = simplify(e);
    e1.accept(&infers);
    
    std::vector<DomInterval> &result = infers.callee;
    
    // Check for default processing (unhandled tree nodes) and update exact flag
    for (int j = Domain::Valid; j < Domain::MaxDomains; j++) {
        //if (result[j].imin.defined()) result[j].imin = simplify(result[j].imin);
        //if (result[j].imax.defined()) result[j].imax = simplify(result[j].imax);
        if (infers.defaulted) result[j].exact = false; // Unhandled tree node - result is not exact.
        
        // If it is known that the expression is not exact, then the range is set to infinite.
        // This is because whatever information was computed is incomplete and could be incorrect.
        log(2,"DOMINF") << "Result[" << j << "]: " << result[j] << "\n";
        if (! result[j].exact) {
            assert(result[j].min.defined() && result[j].max.defined());
            result[j].min = make_infinity(result[j].min.type(), -1);
            result[j].max = make_infinity(result[j].max.type(), +1);
        }
    }
    
    // Special case.  If the expression contains only one implicit variable occurring once then the semantics is
    // kernel semantics.  This is a bit of a hack: it is possible for a Halide program to mimic this behaviour, but
    // Halide program writers should never use implicit variables explicitly.
    std::vector<std::string> vlist = list_repeat_variables(e);
    bool kernel_semantics = (int) vlist.size() == 1 && starts_with(vlist[0], "iv.");
    
    // Implementation of kernel semantics
    if (kernel_semantics) {
        log(2) << "Kernel semantics apply\n";
        log(2) << "result[0]: " << result[0] << "  callee[0]: " << callee[0] <<"\n";
        // If kernel semantics apply, then the computable domain is whatever was computed by
        // backwards interval analysis, but the valid domain is copied from the callee and then is intersected
        // with the computable domain.
        result[Domain::Valid] = callee[Domain::Valid];
        result[Domain::Valid] = intersection(result[Domain::Valid], result[Domain::Computable]); // Intersect with the computable domain
    }
    else
        log(2) << "Kernel semantics do not apply\n";
        
    if (varname)
        *varname = infers.varname; // Return the found variable name if the caller wants to know it.
    
    return result;
}



std::vector<DomInterval> backwards_interval(const std::vector<std::string> &varlist, std::vector<Domain> &domains, Expr e, DomInterval xint, std::string *varname = NULL) {
    DomInterval callee(xint);
    std::vector<DomInterval> intervals;
    // Expand a single interval to represent all the different domain types.
    // This applies to images and to testing.
    for (int j = Domain::Valid; j < Domain::MaxDomains; j++) {
        intervals.push_back(callee);
        log(2) << "backward interval initialisation [" << j << "]: " << intervals[j] << '\n';
    }
    return backwards_interval(varlist, domains, e, intervals, varname);
}





// ForwardDomainInference walks the parse tree inferring forward domain bounds
// for functions.
// This operates on the very raw parse tree before functions are realized etc
// so that we can interpret the index expressions.

// Class ForwardDomainInference is only for internal use in this module.
class ForwardDomainInference : public IRVisitor {
public:
    std::vector<Domain> domains; // The domain that we are building.
    const std::vector<std::string> &varlist;
    
    ForwardDomainInference(const std::vector<std::string> &variables) : varlist(variables) {
        // Construct initial domains from scratch
        domains.clear();
        for (int j = Domain::Valid; j < Domain::MaxDomains; j++) {
            domains.push_back(Domain()); // An empty domain
            for (size_t i = 0; i < variables.size(); i++) {
                // Initial intervals are infinite and exact
                // If no constraints end up being applied, then that is the correct domain.
                domains[j].intervals.push_back(DomInterval(make_infinity(Int(32), -1), make_infinity(Int(32), +1), true));
            }
        }
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
        
        // If it is an external function, then it is not of interest here.
        // It acts in the same way as any other expression.
        if (func_call->call_type == Call::Extern) {
            IRVisitor::visit(func_call);
            return;
        }
        
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
            //log(0,"LH") << "arg " << i << ": " << func_call->args[i] << '\n';
            
            // Perform backward interval analysis on the argument expression.
            // Have to know the valid domain for the actual argument of the function.
            // For images, just use image dimensions.  All the different types of domain are the same
            // for images.
            std::vector<DomInterval> result;
            std::string result_varname;
            if (func_call->call_type == Call::Image)
            {
                if (func_call->image.defined()) {
                    // All domains are the same for an image.
                    log(2,"DOMINF") << "Domain Inference on Image buffer " << func_call->image.name() << "\n";
                    result = backwards_interval(varlist, domains, func_call->args[i],
                                            DomInterval(func_call->image.min(i), 
                                            func_call->image.min(i) + func_call->image.extent(i) - 1, true), &result_varname);
                }
                else if (func_call->param.defined()) {
                    log(2,"DOMINF") << "Domain Inference on Image parameter " << func_call->param.name() << "\n";
                    result = backwards_interval(varlist, domains, func_call->args[i],
                                            DomInterval(func_call->param.min(i), 
                                            func_call->param.min(i) + func_call->param.extent(i) - 1, true), &result_varname);
                }
                else
                    assert(0 && "Call to Image is neither image nor imageparam\n");
            }
            else if (func_call->call_type == Call::Halide)
            {
                // For Halide calls, access the domain in the function object.
                // We need to know the DomInterval of the i'th parameter of the called function.
                log(2,"DOMINF") << "Domain Inference on Halide call to " << func_call->func.name() << "\n";
                log(4,"DOMINF") << "Variables: ";
                for (size_t ii = 0; ii < varlist.size(); ii++) { log(4,"DOMINF") << varlist[ii] << " "; }
                log(4,"DOMINF") << "\n";
                result = backwards_interval(varlist, domains, func_call->args[i],
                                        func_call->func.domain_intervals(i), &result_varname);
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
                for (int j = Domain::Valid; j < Domain::MaxDomains; j++) {
                    int index = find(varlist, result_varname);
                    if (index >= 0) {
                        domains[j].intervals[index] = intersection(domains[j].intervals[index], result[j]);
                    }
                    else if (! result_varname.empty()) { // Empty varname means no interval information.
                        log(0) << "Warning: The interval variable " << result_varname << " is unknown\n";
                    }
                }
                
            }
        }
    }
};



#define USE_SOLVER 1

/* Notes
Difference between Var and Variable.  Variable is a parse tree node.
Var is just a name.
*/
# if ! USE_SOLVER
std::vector<Domain> domain_inference(const std::vector<std::string> &variables, Expr e)
# else
std::vector<Domain> old_domain_inference(const std::vector<std::string> &variables, Expr e)
# endif
{
    log(1,"DOMINF") << "domain_inference: Expression: " << e << "\n";
    // At this level, we use a list of variables passed in.
    ForwardDomainInference infers(variables);
    
    assert(e.defined() && "domain_inference applied to undefined expression");
    
    e.accept(&infers);
    
    log(1,"DOMINF") << "Domain returned: \n";
    for (int j = Domain::Valid; j < Domain::MaxDomains; j++) {
        for (size_t i = 0; i < infers.domains[j].intervals.size(); i++) {
            log(1,"DOMINF") << "[" << j << "]: " << variables[i] << ": " << infers.domains[j].intervals[i] << "\n";
        }
    }
    
    return infers.domains;
}




/** Prepare for domain inference. Recognise trigger expressions and insert Solve nodes. 
 * Does not insert TargetVar nodes because the target variables are, in fact, predetermined.
 */
class DomainPreSolver : public InlineLet {
    std::vector<std::string> variables;
public:
    DomainPreSolver(const std::vector<std::string>& _variables) : variables(_variables) {}
    
    Expr presolve(Expr e) {
        // First mutate the entire tree
        Expr m = mutate(e);
        // Then insert TargetVar statements above
        for (size_t i = 0; i < variables.size(); i++) {
            m = TargetVar::make(variables[i], m, Expr());
        }
        return m;
    }
     
    
protected:
    using InlineLet::visit;
    
    // Call nodes are the places that trigger domain inference.
    virtual void visit(const Call *func_call) {
        // Arguments are in std::vector<Expr> func_call->args
        // If it is a call to another Halide function, Function func_call->func points to it
        // If it is a direct reference to an image, Buffer func_call->image is the buffer
        // and if it is an image parameter, Parameter func_call->param is that.
        // To check use ->func.value().defined(), ->image.defined() and ->param.defined().
        
        // If it is not an image, nor a Halide function, then it is not of interest
        // here. It acts like any other expression element.
        if (func_call->call_type != Call::Image && func_call->call_type != Call::Halide) {
            InlineLet::visit(func_call);
            return;
        }
        
        // Each of the argument expressions must be processed in turn.
        std::vector<Expr> newargs;
        for (size_t i = 0; i < func_call->args.size(); i++)
        {
            std::vector<DomInterval> domain;
            DomInterval interval;
            
            if (func_call->call_type == Call::Image)
            {
                if (func_call->image.defined()) {
                    interval = DomInterval(func_call->image.min(i), 
                                            func_call->image.min(i) + func_call->image.extent(i) - 1, true);
                }
                else if (func_call->param.defined()) {
                    interval = DomInterval(func_call->param.min(i), 
                                            func_call->param.min(i) + func_call->param.extent(i) - 1, true);
                }
                else {
                    assert(0 && "Call to Image is neither image nor imageparam\n");
                }
                // All domains are the same for an image.
                domain.push_back(interval);
                domain.push_back(interval);
            }
            else if (func_call->call_type == Call::Halide)
            {
                // For Halide calls, access the domain in the function object.
                // We need to know the DomInterval of the i'th parameter of the called function.
                domain = func_call->func.domain_intervals(i);
            }
            newargs.push_back(Solve::make(func_call->args[i], domain));
        }
        expr = Call::make(func_call, newargs);
        return;
    }
};

#if USE_SOLVER
// TO DO: CHANGE TO ACCEPT A FUNCTION DEFINITION?  (recurrent/reducing functions)
// OR SHOULD IT BE THAT the domain from the main definition becomes the domain of the
// recurrent result computation.
std::vector<Domain> domain_inference(const std::vector<std::string> &variables, Expr e)
{
    log(1,"DOMINF") << "domain_inference: Expression: " << e << "\n";
    assert(e.defined() && "domain_inference applied to undefined expression");

    // At this level, we use a list of variables passed in.
    code_logger.section("pre_dominf");
    Expr pre = DomainPreSolver(variables).presolve(e);
    code_logger.log(pre, "pre_dominf");
    
    code_logger.section("solved_dominf");
    Expr solved = domain_solver(pre);
    code_logger.log(solved, "solved_dominf");
    
    // Extract the solutions and compute the domains.

    // Create a vector of Domain: One Domain for each of Valid and Computable.
    // Each domain contains a vector intervals, one DomInterval for each variable.
    std::vector<Domain> result;
    for (int dt = Domain::Valid; dt < Domain::MaxDomains; dt++) {
        std::vector<DomInterval> intervals;
        
        for (size_t i = 0; i < variables.size(); i++) {
            std::vector<Solution> solutions;
            bool exact;
            solutions = extract_solutions(variables[i], Expr(), solved, &exact);
            
            log(3,"DOMINF") << "Solutions:\n";
            for (size_t j = 0; j < solutions.size(); j++) {
                log(3,"DOMINF") << solutions[j].intervals << "\n";
            }
            // Intersect all the solutions to get the actual domain interval.
            DomInterval interval(Int(32), Expr(), Expr(), exact);
            for (size_t j = 0; j < solutions.size(); j++) {
                assert(solutions[j].intervals.size() >= (size_t) dt && "FAILURE: Solution intervals vector is not large enough");
                interval = intersection(interval, solutions[j].intervals[dt]);
            }
            intervals.push_back(interval);
        }
        result.push_back(Domain(intervals));
    }
    
    // Intersect the valid domain with the computable.
    //result[Domain::Valid] = result[Domain::Valid].intersection(result[Domain::Computable]);
    
    log(2,"DOMINF") << "Valid: " << result[Domain::Valid] << "\n";
    log(2,"DOMINF") << "Computable: " << result[Domain::Computable] << "\n";
    
    
    std::vector<Domain> old_result = old_domain_inference(variables, e);
    
    assert(old_result.size() == result.size() && old_result.size() == Domain::MaxDomains);
    for (int dt = Domain::Valid; dt < Domain::MaxDomains; dt++) {
        assert(old_result[dt].intervals.size() == result[dt].intervals.size() && old_result[dt].intervals.size() == variables.size());
        for (size_t i = 0; i < variables.size(); i++) {
            // Check old_result vs result
            if (! equal(result[dt].intervals[i].min, old_result[dt].intervals[i].min) ||
                ! equal(result[dt].intervals[i].max, old_result[dt].intervals[i].max) ||
                result[dt].intervals[i].exact != old_result[dt].intervals[i].exact) {
                std::cerr << "Mismatch in domain inference: solver: " << result[dt].intervals[i] << "  old: " << old_result[dt].intervals[i] << "\n";
                std::cerr << "    domain: " << (dt == Domain::Valid ? "valid" : "computable")  << "  variable: " << variables[i] << "\n";
                std::cerr << "    expression: " << e << "\n";
                assert(0);
            }
        }
    }
    
    return old_result;
}
#endif





void check_interval(std::vector<std::string> varlist, Expr e, Expr xmin, Expr xmax, 
                    bool correct_exact, Expr correct_min, Expr correct_max, 
                    std::string correct_varname) {
    Domain dom = Domain(DomInterval(Int(32), Expr(), Expr(), true), 
                        DomInterval(Int(32), Expr(), Expr(), true)); // A working domain for the test variables x and y
    std::vector<Domain> doms = vec(dom, dom);
    std::string result_varname;
    std::vector<DomInterval> result = backwards_interval(varlist, doms, e, DomInterval(xmin, xmax, true), &result_varname);
    
    Expr e1 = simplify(e); // Duplicate simplification for debugging only
    log(1,"DOMINF") << "e: " << e << "    ";
    log(2,"DOMINF") << "e1: " << e1 << "    ";
    
    if (! result[Domain::Valid].exact)
        log(1,"DOMINF") << "inexact\n";
    else {
        log(1,"DOMINF") << "exact min: " << result[Domain::Valid].min << "    ";
        log(1,"DOMINF") << "max: " << result[Domain::Valid].max << "    ";
        log(1,"DOMINF") << "v: " << result_varname << "\n";
    }
    
    bool success = true;
    if (! (result[Domain::Valid].exact == correct_exact)) {
        std::cout << "Incorrect exact: " << result[Domain::Valid].exact << "    "
                  << "Should have been: " << correct_exact << std::endl;
        success = false;
    }
    if (correct_exact) {
        // Only bother to check the details if it is supposed to be exact
        if (!equal(result[Domain::Valid].min, correct_min)) {
            std::cout << "Incorrect min: " << result[Domain::Valid].min << "    "
                      << "Should have been: " << correct_min << std::endl;
            success = false;
        }
        if (!equal(result[Domain::Valid].max, correct_max)) {
            std::cout << "Incorrect max: " << result[Domain::Valid].max << "    "
                      << "Should have been: " << correct_max << std::endl;
            success = false;
        }
        if (result_varname != correct_varname) {
            std::cout << "Incorrect variable name: " << result_varname << "    "
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
    check_interval(varlist, x, 0, 100, true, 0, 100, "x");
    check_interval(varlist, x + 1, 0, 100, true, -1, 99, "x");
    check_interval(varlist, 1 + x, 0, 100, true, -1, 99, "x");
    check_interval(varlist, 1 + x + 1, 0, 100, true, -2, 98, "x");
    check_interval(varlist, x - 1, 0, 100, true, 1, 101, "x");
    check_interval(varlist, 1 - x, 0, 100, true, -99, 1, "x");
    //check_interval(varlist, x + x, 0, 100, cmin, cmax, v);
    // Tests that use * and / should ensure that results are positive
    // so that positive remainder semantics hold for division
    // (until these semantics are actually implemented in Halide)
    check_interval(varlist, 2 * x, 10, 100, true, 5, 50, "x");
    check_interval(varlist, x * 2, 10, 100, true, 5, 50, "x");
    check_interval(varlist, x / 2, 10, 100, true, 20, 201, "x");
    // x = 19  e = (19 + 1) / 2 = 10
    // x = 200  e = (201) / 2 = 100
    check_interval(varlist, (x + 1) / 2, 10, 100, true, 19, 200, "x");
    check_interval(varlist, (x + 2) / 2, 10, 100, true, 18, 199, "x");
    // (2 * x + 4) / 2 is the same as x + 2
    check_interval(varlist, (2 * x + 4) / 2, 10, 100, true, 8, 98, "x");
    // x = 8  e = (16 + 5) / 2 = 10
    // x = 98  e = (196 + 5) / 2 = 201 / 2 = 100
    // This expression also simplifies to x + 2
    check_interval(varlist, (2 * x + 5) / 2, 10, 100, true, 8, 98, "x");
    // x = 5  e = (15 + 5) / 2 = 10
    // x = 65  e = (195 + 5) / 2 = 100   but x = 66 is too big
    check_interval(varlist, (3 * x + 5) / 2, 10, 100, true, 5, 65, "x");
    // x = 7  e = (21 + 5) / 2 - 2 = 11  but x=6 e=(18+5)/2-2 = 9
    // x = 66  e = (198 + 5) / 2 - 2 = 99   but x=67 e=(201+5)/2-2=101
    check_interval(varlist, (3 * x + 5) / 2 - 2, 10, 100, true, 7, 66, "x");
    
    // Constant expressions return an infinite domain because there is no constraint
    // on the caller's variables.
    check_interval(varlist, Expr(5) + 7, 0, 100, false, Expr(), Expr(), "");
    check_interval(varlist, 105, 0, 100, false, Expr(), Expr(), "");
    // Expression is inexact because it contains a node type that 
    // is not explicitly handled
    check_interval(varlist, sin(x), 10, 100, false, 0, 0, "");
    // Expression is inexact because it contains more than one variable
    // Actually, it is detected as inexact by Add node because it is not x + k
    check_interval(varlist, x + y, 0, 100, false, 0, 0, "");
    return;
}




void check_domain_expr(Domain::DomainType dtype, std::vector<std::string> variables, Expr e, Domain d) {
    // Compute the domain of the expression using forward domain inference.
    std::vector<Domain> edom = domain_inference(variables, e);
    int j = (int) dtype;
    
    log(1,"DOMINF") << "e: " << e << '\n';
    for (size_t i = 0; i < edom[j].intervals.size(); i++)
        log(1,"DOMINF") << "   (" << i
                    << ") min: " << edom[j].intervals[i].min 
                    << "  max: " << edom[j].intervals[i].max 
                    << "  exact: " << edom[j].intervals[i].exact << '\n';
    
    // Compare the computed domain with the expected domain
    bool success = true;
    if (d.intervals.size() != edom[j].intervals.size()) {
        std::cout << "Incorrect domain size: " << edom[j].intervals.size()
                  << "    Should have been: " << d.intervals.size() << '\n';
        success = false;
    }
    for (size_t i = 0; i < edom[j].intervals.size(); i++) {
        if (! (edom[j].intervals[i].exact == d.intervals[i].exact)) {
            std::cout << "Incorrect exact: " << edom[j].intervals[i].exact
                      << "    Should have been: " << d.intervals[i].exact << '\n';
            success = false;
        }
        // Check the numeric range even if it is not exact
        if (! equal(edom[j].intervals[i].min, d.intervals[i].min)) {
            std::cout << "Incorrect min: " << edom[j].intervals[i].min
                      << "    Should have been: " << d.intervals[i].min << '\n';
            success = false;
        }
        if (! equal(edom[j].intervals[i].max, d.intervals[i].max)) {
            std::cout << "Incorrect max: " << edom[j].intervals[i].max
                      << "    Should have been: " << d.intervals[i].max << '\n';
            success = false;
        }
    }
    
    if (! success) {
        std::cout << "Expression: " << e << '\n';
        for (size_t i = 0; i < edom[j].intervals.size(); i++)
            std::cout << "    (" << i 
                      << ") min: " << edom[j].intervals[i].min 
                      << "  max: " << edom[j].intervals[i].max 
                      << "  exact: " << edom[j].intervals[i].exact << '\n';
    }
    assert(success && "Domain inference test failed");
}

void domain_expr_test()
{
    Image<uint8_t> in(20,40);
    Image<uint8_t> inb(30,35);
    Func f("fred"), g("gold"), h("ham"), fa("fa"), fb("fb"), fc("fc");
    Var x("x"), y("y"), a("a"), b("b"), ext("fff.extent.0");
    Expr False = Internal::make_bool(false);
    Expr True = Internal::make_bool(true);

    check_domain_expr(Domain::Valid, vecS("iv.0", "iv.1"), in, Domain(0, 19, 0, 39));
    check_domain_expr(Domain::Valid, vecS("x", "y"), in(x-2,y), Domain(2, 21, 0, 39));
    check_domain_expr(Domain::Valid, vecS("x", "y"), in(x-2,y) + in(x,y), Domain(2, 19, 0, 39));
    check_domain_expr(Domain::Valid, vecS("x", "y"), in(x-2,y) + in(x,y) + in(x,y+5), 
                        Domain(2, 19, 0, 34));
    // Exact result including use of min function.  min(y+5,15) limits the upper range of y+5 to 15 so it
    // ensures that it remains in bounds at the upper end.  However, as a border handler, it means that
    // the valid domain of y is limited to 10.
    check_domain_expr(Domain::Valid, vecS("x", "y"), in(x-2,y) + in(x,y) + in(x,min(y+5,15)), 
                        Domain(2, 19, 0, 10));
    // Exact results including use of max and min functions.  min(x,9) limits x <= 9 for valid domain.
    // max(x,0) limits x >= 0 for valid domain.  max(y,1) limits y >= 1 for valid domain.
    check_domain_expr(Domain::Valid, vecS("x", "y"), in(x-2,max(y,1)) + in(max(x,0),y) + in(min(x,9),y+5), 
                        Domain(2, 9, 1, 34));
    // Test interchange of variables (flip the domain of the function)
    check_domain_expr(Domain::Valid, vecS("x", "y"), in(y,x), 
                        Domain(0, 39, 0, 19));
    // Test an external function.
    check_domain_expr(Domain::Valid, vecS("x", "y"), sin(in(x,y)),
                        Domain(0, 19, 0, 39));
    // Test multiple use of the same variable
    check_domain_expr(Domain::Valid, vecS("x"), in(x,x), 
                        Domain(0, 19));
    // Test mixture of variables
    check_domain_expr(Domain::Valid, vecS("x", "y"), in(x+y,y), 
                        Domain(DomInterval(Int(32), Expr(), Expr(), false), DomInterval(0, 39, false)));
    // Test domain of a constant expression
    check_domain_expr(Domain::Valid, vecS("x", "y"), 3, 
                        Domain::infinite(2));
    // Test domain with a constant variable - treat it as though it were not a variable
    check_domain_expr(Domain::Valid, vecS("x", "y"), ext, 
                        Domain::infinite(2));
    // Test domain with a constant variable - treat it as though it were not a variable
    // Expressions will include the constant variable
    check_domain_expr(Domain::Valid, vecS("x", "y"), in(x - ext, y), 
                        Domain(ext, ext + 19, 0, 39));

    f(x,y) = in(x-1,y) - in(x,y);
    check_domain_expr(Domain::Valid, vecS("x","y"), f(x,y-1), Domain(1, 19, 1, 40));
    
    // Function g is in with border replication simulated by clamp with manual setting of computable and valid regions
    g(x,y) = in(clamp(x,0,in.width()-1), clamp(y,0,in.height()-1)); // Old way to clamp
	// Creating an infinite domain:
    // We use Domain::infinite with the dimensionality of the function as an argument.
    g.set_computable() = Domain::infinite(g.dimensions()); // Set the computable region first (in case we intersect valid with it)
    g.set_valid() = in.valid(); // Manually update the valid region.
    
    // h is a kernel function of g, using the border handling
    h(a,b) = g(a,b)+g(a,b-1)+g(a,b+1);
    h.local(g); // h is a kernel function of g, so the valid region is copied and intersected with computable region
    check_domain_expr(Domain::Valid, vecS("x","y"), h(x,y), Domain(0, 19, 0, 39));
    //check_domain_expr(Domain::Computable, vecS("x","y"), h(x,y), Domain("x", False, Expr(), Expr(), "y", False, Expr(), Expr()));

    // fa is a kernel function of both g and inb
    // Note that the computable domain of inb limits the computable domain of fa
    fa(x,b) = g(x-1,b+1) + inb(x,b-2);
    // Before declaring fa to be kernel function, the automatically derived domains should be as follows.
    // The shifts of the valid domains have impacted the final valid domain, and the computable domain is 
    // also influenced by shifting of inb.
    check_domain_expr(Domain::Valid, vecS("x","y"), fa(x,y), Domain(1, 20, 2, 36));
    //check_domain_expr(Domain::Computable, vecS("x","y"), fa(x,y), Domain("x", False, 0, 29, "y", False, 2, 36));
    
    // The following is not valid because fa() has been used already for domain inference.
    //fa.kernel_of(g,inb);
    Func ffa("ffa");
    //ffa = fa();
    ffa = fa();
    ffa.local(g,inb);
    // Declaring fa to be a kernel function overrides the shifts implicit in the definition of fa.
    // For g, this means that the valid domain is copied but for inb the computable domain is restricting the valid
    // domain.
    check_domain_expr(Domain::Valid, vecS("x","y"), ffa(x,y), Domain(0, 19, 2, 34));
    //check_domain_expr(Domain::Computable, vecS("x","y"), fa(x,y), Domain("x", False, 0, 29, "y", False, 2, 36));
    return;
}

void domain_inference_test()
{
    backward_interval_test();
    //std::cout << "Backward interval test passed" << std::endl;
    domain_expr_test();
    std::cout << "Domain inference test passed" << std::endl;
}

}
}
