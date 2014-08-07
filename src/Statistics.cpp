#include "Statistics.h"
#include "Options.h"


namespace Halide {

// Declare the global object that is used throughout by the compiler.
Statistics global_statistics;

// Constructor initialises the statistics, usually to zero.
Statistics::Statistics() {
	mutator_cache_hits = 0;
    mutator_cache_misses = 0;
    mutator_cache_savings = 0;
}

void Statistics::Subtract(const Statistics &other) {
    mutator_cache_hits -= other.mutator_cache_hits;
    mutator_cache_misses -= other.mutator_cache_misses;
    mutator_cache_savings -= other.mutator_cache_savings;
}


std::ostream &operator<<(std::ostream &stream, Statistics opt) {
	stream << "Mutator cache hits=" << opt.mutator_cache_hits << " misses=" << opt.mutator_cache_misses;
    // Savings is not meaningful when mutator_cache_check is false, but if it is non-zero then that
    // means that mutator_cache_check was true in the past so report the savings.
    if (global_options.mutator_cache_check || opt.mutator_cache_savings != 0) {
        stream << " savings=" << opt.mutator_cache_savings;
    }
    stream << "\n";
    return stream;
}


// end namespace Halide
}

