#include "Options.h"


namespace Halide {

// Declare the global object that is used throughout by the compiler.
Options global_options;

// Constructor defines the default values for each option.
// Options may be freely added provided that the default
// value causes the compiler to act in the way it did
// before the option was introduced.
// Default values may be changed when testing shows that
// there is a better default.  Changing a default is the
// same as updating the compiler itself.
Options::Options() {
	border_value_inner_outside = true; // Inner outside is faster on X86, LLVM 3.2.
	clamp_as_node = false; // Default is to use Halide clamp implementation immediately.
	simplify_nested_clamp = true; // Default is to simplify nested clamp because the code is auto-tested.
	loop_split = true; // Default is to do loop partitioning.
    loop_split_all = false; // Default is not to partition loop automatically - some code fails.
    loop_split_letbind = true;
    lift_let = true;
	interval_analysis_simplify = true;
    mutator_depth_limit = 1000;
    mutator_cache = true;
    mutator_cache_check = false;
    mutator_cache_check_limit = 100000;
    simplify_shortcuts = true; // Seems to speed up compilation
    simplify_lift_constant_min_max = false;
}


std::ostream &operator<<(std::ostream &stream, Options opt) {
	stream << "border_value_inner_outside=" << opt.border_value_inner_outside
	       << "    clamp_as_node=" << opt.clamp_as_node << "\n";
    stream << "loop_split=" << opt.loop_split
           << "    loop_split_all=" << opt.loop_split_all
           << "    loop_split_letbind=" << opt.loop_split_letbind << "\n";
    stream << "lift_let=" << opt.lift_let
	       << "    interval_analysis_simplify=" << opt.interval_analysis_simplify << "\n";
    stream << "mutator_cache=" << opt.mutator_cache << "\n";
    stream << "simplify: shortcuts=" << opt.simplify_shortcuts << "    lift_constant_min_max=" << opt.simplify_lift_constant_min_max << "\n";
    return stream;
}


// end namespace Halide
}

