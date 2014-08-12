#ifndef HALIDE_FUNCTION_H
#define HALIDE_FUNCTION_H

/** \file
 * Defines the internal representation of a halide function and related classes
 */

#include "IntrusivePtr.h"
#include "Reduction.h"
#include "Schedule.h"
#include <string>
#include <vector>

//LH
#include "DomainInference.h"

namespace Halide { 
namespace Internal {

struct FunctionContents {
    mutable RefCount ref_count;
    std::string name;
    std::vector<std::string> args;
    Expr value;
    Schedule schedule;
	
	//LH
	// Forward domain inference and domain manipulation for border handling.
	// The valid domain is the domain over which this function is useful and meaningful.
	// For example, the valid domain of an edge detector would normally be the same as the input image
	// with border handling used to provide useful results at the borders of the image.
	// The computable domain is the domain over which this function is known to be computable.
	// For example, the valid domain may be extended indefinitely by replicating the border pixels.
	std::vector<Domain> domains;

    Expr reduction_value;
    std::vector<Expr> reduction_args;
    Schedule reduction_schedule;
    ReductionDomain reduction_domain;

    std::string debug_file;
};        

/** A reference-counted handle to Halide's internal representation of
 * a function. Similar to a front-end Func object, but with no
 * syntactic sugar to help with definitions. */
class Function {
private:
    IntrusivePtr<FunctionContents> contents;
public:
    /** Construct a new function with no definitions and no name. This
     * constructor only exists so that you can make vectors of
     * functions, etc.
     */
    Function() : contents(new FunctionContents) {}

    /** Add a pure definition to this function. It may not already
     * have a definition. All the free variables in 'value' must
     * appear in the args list. 'value' must not depend on any
     * reduction domain */
    void define(const std::vector<std::string> &args, Expr value);   

    /** Add a reduction definition to this function. It must already
     * have a pure definition but not a reduction definition, and the
     * length of args must match the length of args used in the pure
     * definition. 'value' must depend on some reduction domain, and
     * may contain variables from that domain as well as pure
     * variables. Any pure variables must also appear as Variables in
     * the args array, and they must have the same name as the pure
     * definition's argument in the same index. */
    void define_reduction(const std::vector<Expr> &args, Expr value);

    /** Construct a new function with the given name */
    Function(const std::string &n) : contents(new FunctionContents) {
        contents.ptr->name = n;
    }

    /** Get the name of the function */
    const std::string &name() const {
        return contents.ptr->name;
    }

    /** Get the pure arguments */
    const std::vector<std::string> &args() const {
        return contents.ptr->args;
    }

    /** Get the right-hand-side of the pure definition */
    Expr value() const {
        return contents.ptr->value;
    }

    /** Get a handle to the schedule for the purpose of modifying
     * it */
    Schedule &schedule() {
        return contents.ptr->schedule;
    }   

    /** Get a const handle to the schedule for inspecting it */
    const Schedule &schedule() const {
        return contents.ptr->schedule;
    }   
    
	//LH
	/** Get a handle to a domain for the purpose of modifying it */
	Domain &set_domain(Domain::DomainType dt);

	//LH
	/** Get a handle to a domain for the purpose of inspecting it */
	const Domain &domain(Domain::DomainType dt) const;
    
    //LH
    /** Get the corresponding interval of all the domains, for a particular index */
    const std::vector<DomInterval> domain_intervals(int index) const;

    /** Get a mutable handle to the schedule for the reduction
     * stage */
    Schedule &reduction_schedule() {
        return contents.ptr->reduction_schedule;
    }

    /** Get a const handle to the schedule for the reduction stage */
    const Schedule &reduction_schedule() const {
        return contents.ptr->reduction_schedule;
    }

    /** Get the right-hand-side of the reduction definition */
    Expr reduction_value() const {
        return contents.ptr->reduction_value;
    }

    /** Get the left-hand-side of the reduction definition */
    const std::vector<Expr> &reduction_args() const {
        return contents.ptr->reduction_args;        
    }

    /** Get the reduction domain for the reduction definition */
    ReductionDomain reduction_domain() const {
        return contents.ptr->reduction_domain;
    }

    /** Is this function a reduction? */
    bool is_reduction() const {
        return reduction_value().defined();
    }

    /** Equality of identity */
    bool same_as(const Function &other) const {
        return contents.same_as(other.contents);
    }

    /** Get a const handle to the debug filename */
    const std::string &debug_file() const {
        return contents.ptr->debug_file;
    }

    /** Get a handle to the debug filename */
    std::string &debug_file() {
        return contents.ptr->debug_file;
    }
};

}}

#endif
