#ifndef HALIDE_STATISTICS_H
#define HALIDE_STATISTICS_H

#include <iostream>

namespace Halide {
/** Class that is used to report compilation statistics.
 */
class Statistics {
public:
	// The constructor sets the default values.
	Statistics();
	// Subtract: Subtracts other statistics from an object.
    // Use this to compute the local statistics for an individual compilation.
    void Subtract(const Statistics &other);

    // mutator_cache_hits, mutator_cache_misses: Counts of accesses to
    // IRCacheMutator::mutate() that were hits/misses.  
    // If global_options.mutator_cache is false then all will be misses.
    // If global_options.mutator_cache_check is true, then mutator_cache_savings
    // will indicate the number of cache misses saved by enabling hits.
    int mutator_cache_hits, mutator_cache_misses, mutator_cache_savings;
};

std::ostream &operator<<(std::ostream &stream, Statistics opt);

extern Statistics global_statistics;

// end namespace Halide
}
#endif
