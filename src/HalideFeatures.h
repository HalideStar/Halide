#ifndef HALIDE_FEATURES_H
#define HALIDE_FEATURES_H

# define HALIDE_DOMAIN_INFERENCE 1
// Introduce the Clamp node used for border handing
# define HALIDE_CLAMP_NODE

// Adjust reference counts for circular references
// Disable this option to overcome a bug that appears in the test
// circular_reference_leak.cpp.
//# define HALIDE_CIRCULAR_REFERENCE_ADJUST

#endif
