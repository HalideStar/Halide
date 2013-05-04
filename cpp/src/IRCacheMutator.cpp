#include "IRCacheMutator.h"
#include "Log.h"
#include "IRPrinter.h"
#include "IREquality.h"
#include "Options.h"
#include "Statistics.h"

namespace Halide {
namespace Internal {

template<typename Node>
Node IRCacheMutator::mutate(Node node, Node &result) {
    if (global_options.mutator_cache) {
        // First check to see whether there is a result in the cache.
        // Since we have not yet processed the node (i.e. we have not entered
        // the process method), the context is the enclosing context
        // of the node, and this yields an appropriate key for looking up the node.
        NodeKey key = node_key(node);
        typename CacheMap::const_iterator found = cache.find(key);
        if (found != cache.end()) {
            global_statistics.mutator_cache_hits++;
            if (global_options.mutator_cache_check && 
                global_statistics.mutator_cache_hits + global_statistics.mutator_cache_misses + 
                global_statistics.mutator_cache_savings < global_options.mutator_cache_check_limit) {
                int misses = global_statistics.mutator_cache_misses;
                int hits = global_statistics.mutator_cache_hits;
                // Check the cache for correctness by recomputing.
                Node temp = Super::mutate(node);
                assert (equal(temp, found->second) && "Cache error.");
                // After checking, there will be additional misses.  These are the saved misses.
                // In addition, there may be additional hits which are also savings.
                global_statistics.mutator_cache_savings += global_statistics.mutator_cache_misses - misses;
                global_statistics.mutator_cache_misses = misses;
                global_statistics.mutator_cache_savings += global_statistics.mutator_cache_hits - hits;
                global_statistics.mutator_cache_hits = hits;
            }
            result = found->second;
            return result;
        } else {
            global_statistics.mutator_cache_misses++;
            result = Super::mutate(node);
            cache[key] = CachedNode(result);
            return result;
        }
    } else {
        global_statistics.mutator_cache_misses++;
        result = Super::mutate(node);
        return result;
    }
}

Expr IRCacheMutator::mutate(Expr e) {
    stmt = Stmt();
    if (! e.defined()) {
        expr = Expr();
        return expr;
    }
    return IRCacheMutator::mutate(e, expr);
}

Stmt IRCacheMutator::mutate(Stmt s) {
    expr = Expr();
    if (! s.defined()) {
        stmt = Stmt();
        return stmt;
    }
    return IRCacheMutator::mutate(s, stmt);
}

// end namespace Internal
}
}
