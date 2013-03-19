#include "IR.h"
#include "Function.h"
#include "Scope.h"

//LH
#include "log.h"

namespace Halide {
namespace Internal {

using std::vector;
using std::string;

template<>
EXPORT RefCount &ref_count<FunctionContents>(const FunctionContents *f) {return f->ref_count;}

template<>
EXPORT void destroy<FunctionContents>(const FunctionContents *f) {delete f;}

// All variables present in any part of a function definition must
// either be pure args, elements of the reduction domain, parameters
// (i.e. attached to some Parameter object), or part of a let node
// internal to the expression
struct CheckVars : public IRVisitor {
    vector<string> pure_args;
    ReductionDomain reduction_domain;
    Scope<int> defined_internally;

    using IRVisitor::visit;

    void visit(const Let *let) {
        defined_internally.push(let->name, 0);
        let->value.accept(this);
        defined_internally.pop(let->name);
    }

    void visit(const Variable *var) {
        // Is it a parameter?
        if (var->param.defined()) return;

        // Was it defined internally by a let expression?
        if (defined_internally.contains(var->name)) return;

        // Is it a pure argument?
        for (size_t i = 0; i < pure_args.size(); i++) {
            if (var->name == pure_args[i]) return;
        }

        // Is it in a reduction domain?
        if (var->reduction_domain.defined()) {
            if (!reduction_domain.defined()) {
                reduction_domain = var->reduction_domain;
                return;
            } else if (var->reduction_domain.same_as(reduction_domain)) {
                // It's in a reduction domain we already know about
                return;
            } else {                
                assert(false && "Multiple reduction domains found in function definition");
            }
        }

        std::cerr << "Undefined variable in function definition: " << var->name << std::endl;
        assert(false);
    }
};

void Function::define(const vector<string> &args, Expr value) {
    assert(!name().empty() && "A function needs a name");
    assert(value.defined() && "Undefined expression in right-hand-side of function definition\n");

    // Make sure all the vars in the value are either args or are
    // attached to some parameter
    CheckVars check;
    check.pure_args = args;
    value.accept(&check);

    assert(!check.reduction_domain.defined() && "Reduction domain referenced in pure function definition");

    if (!contents.defined()) {
        contents = new FunctionContents;
        contents.ptr->name = unique_name('f');
    }

    assert(!contents.ptr->value.defined() && "Function is already defined");
    contents.ptr->value = value;
    contents.ptr->args = args;
        
    for (size_t i = 0; i < args.size(); i++) {
        Schedule::Dim d = {args[i], For::Serial};
        contents.ptr->schedule.dims.push_back(d);
        contents.ptr->schedule.storage_dims.push_back(args[i]);
    }        

    //LH
    // Compute forward domain inference.
    log(2,"DI") << "Domain inference for " << name() << "\n";
    contents.ptr->domains = domain_inference(args, value);
}

void Function::define_reduction(const vector<Expr> &args, Expr value) {
    assert(!name().empty() && "A function needs a name");
    assert(contents.ptr->value.defined() && "Can't add a reduction definition without a regular definition first");
    assert(!is_reduction() && "Function already has a reduction definition");
    assert(value.defined() && "Undefined expression in right-hand-side of reduction");

    // Check the dimensionality matches
    assert(args.size() == contents.ptr->args.size() && 
           "Dimensionality of reduction definition must match dimensionality of pure definition");

    // The pure args are those naked vars in the args that are not in
    // a reduction domain and are not parameters
    vector<string> pure_args;
    for (size_t i = 0; i < args.size(); i++) {
        assert(args[i].defined() && "Undefined expression in left-hand-side of reduction");
        if (const Variable *var = args[i].as<Variable>()) {           
            if (!var->param.defined() && !var->reduction_domain.defined()) {
                assert(var->name == contents.ptr->args[i] && 
                       "Pure argument to update step must have the same name as pure argument to initialization step in the same dimension");
                pure_args.push_back(var->name);
            }
        }

    }

    // Make sure all the vars in the args and the value are either
    // pure args, in the reduction domain, or a parameter
    CheckVars check;
    check.pure_args = pure_args;
    value.accept(&check);
    for (size_t i = 0; i < args.size(); i++) {
        args[i].accept(&check);
    }

    assert(check.reduction_domain.defined() && "No reduction domain referenced in reduction definition");

    contents.ptr->reduction_args = args;
    contents.ptr->reduction_value = value;
    contents.ptr->reduction_domain = check.reduction_domain;

    // First add the pure args in order
    for (size_t i = 0; i < pure_args.size(); i++) {
        Schedule::Dim d = {pure_args[i], For::Serial};
        contents.ptr->reduction_schedule.dims.push_back(d);
    }

    // Then add the reduction domain outside of that
    for (size_t i = 0; i < check.reduction_domain.domain().size(); i++) {
        Schedule::Dim d = {check.reduction_domain.domain()[i].var, For::Serial};
        contents.ptr->reduction_schedule.dims.push_back(d);
    }
}

//LH
// Get the corresponding interval of all the domains
const std::vector<VarInterval> Function::domain_intervals(int index) const {
    std::vector<VarInterval> intervals;
    assert(contents.ptr->domains.size() >= Domain::MaxDomains && "Insufficient Domains defined in Function");
    for (int j = 0; j < Domain::MaxDomains; j++) {
        intervals.push_back(contents.ptr->domains[j].intervals[index]);
    }
    return intervals;
}

//LH
/** Get a handle to a domain for the purpose of modifying it */
Domain &Function::domain(Domain::DomainType dt) {
    assert(dt >= 0 && dt < Domain::MaxDomains && "Domain type is not in range");
    assert((size_t) dt < contents.ptr->domains.size() && "Domain of type does not exist");
    log(0) << "Writing domain " << (int) dt << "\n";
    return contents.ptr->domains[dt];
}

//LH
/** Get a handle to a domain for the purpose of inspecting it */
const Domain &Function::domain(Domain::DomainType dt) const {
    assert(dt >= 0 && dt < Domain::MaxDomains && "Domain type is not in range");
    assert((size_t) dt < contents.ptr->domains.size() && "Domain of type does not exist");
    return contents.ptr->domains[dt];
}


}
}
