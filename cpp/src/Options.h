#ifndef HALIDE_OPTIONS_H
#define HALIDE_OPTIONS_H

#define HALIDESTAR 1

#include <iostream>

namespace Halide {
/** Class that is used to select compiler options.  This is a level lower and more
 * detailed than the schedule.  It is intended for use by developers who wish to
 * test out fine grained control of the compiler.
 * The constructor sets options to their current default values as specified by the
 * current version of the compiler.  A user can override any option, and can also reset
 * an option to the default by copying the value from a newly constructed Options object.
 */
class Options {
public:
    // The constructor sets the default values.
    Options();
    
    // border_value_inner_outside: The border value expressions can be built either with the
    // innermost index variable as the inside or outside variable encountered
    // in the expression.  Inner outside (true) means select(x... select(y... )) whereas inner inside (false) means
    // select(y... select(x... ))
    // See border_builder in Border.cpp
    bool border_value_inner_outside;
    
    // clamp_as_node: Should the Clamp node be used to represent clamp expressions (true), or should they
    // be immediately desugared to min and max expressions (false).
    // See clamp() in IROperator.h
    bool clamp_as_node;
    
    // simplify_nested_clamp: Apply simplify rules the handle nested clamp expressions.
    bool simplify_nested_clamp;
    
    // loop_split: If true, enables code for loop splitting (index-set splitting).
    bool loop_split;
    
    // loop_split_all: If true, automatic loop splitting is enabled by default for all code.
    bool loop_split_all;
    
    // loop_split_parallel: If true, loop splitting is enabled for parallel loops.
    bool loop_split_parallel;
    
    // loop_split_letbind: If true, fully let bind the loop bounds.  For speed comparisons.
    bool loop_split_letbind;
    
    // lift_let: If true, lift Let outside of For as much as possible.
    bool lift_let;
    
    // interval_analysis_simplify: If true, simplify loop contents using interval analysis.
    bool interval_analysis_simplify;
    
    // mutator_depth_limit: Limit on depth of recursive mutate calls.
    int mutator_depth_limit;
    
    // mutator_cache: If true, enable caching in IRCacheMutator.  If false, no caching is done.
    bool mutator_cache;
    // mutator_cache_check: If true, verify cached data by recomputing the result and comparing.
    bool mutator_cache_check;
    // mutator_cache_check_limit: Maximum number of additional mutations to perform as checking.
    int mutator_cache_check_limit;
    
    // Simplifier
    bool simplify_shortcuts; // Enable shortcut rules that may speed up compilation.
    bool simplify_lift_constant_min_max; // Enable lifting constants out of min and max
};

std::ostream &operator<<(std::ostream &stream, Options opt);

extern Options global_options;

// end namespace Halide
}
#endif
