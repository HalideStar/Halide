#ifndef HALIDE_FEATURES_H
#define HALIDE_FEATURES_H

# define HALIDE_DOMAIN_INFERENCE 1

// Introduce Border Handling library and support facilities
# define HALIDE_BORDER
# ifdef HALIDE_BORDER
    // Introduce the Clamp node used for border handing
#   define HALIDE_CLAMP_NODE
# endif

// Adjust reference counts for circular references
// Disable this option to overcome a bug that appears in the test
// circular_reference_leak.cpp.
//# define HALIDE_CIRCULAR_REFERENCE_ADJUST

#endif
