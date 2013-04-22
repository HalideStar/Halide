#include "IRCacheMutator.h"
#include "Log.h"
#include "IRPrinter.h"
#include "IREquality.h"
#include "Options.h"
#include "Statistics.h"

namespace Halide {
namespace Internal {

using std::vector;

template <typename Node>
void ContextCache<Node>::push(ContextInfo &info, Node node) {
    assert (node.defined() && "Context push with undefined");
    info.context_stack.push_back(info.context()); // Push current context onto the stack.
    CacheKey<Node> key(info.context(), node); // Key to search for previous push result.
    typename ContextCacheMap<Node>::iterator found = cache.find(key); // Need typename keyword
    if (found == cache.end()) {
        // Not found in the cache. Create a new context and add it to the cache.
        cache[key] = info.new_context();
    } else {
        // Found in the cache.  Use the cached result.
        info.set_context(found->second);
    }
}

template <typename Node>
void ContextCache<Node>::pop(ContextInfo &info, Node node) {
    assert (info.context_stack.size() > 0 && "Context pop with empty stack");
    int old_context = info.context_stack.back(); // Get the last context on the stack
    // Check that the node that we are popping was indeed the one pushed.
    CacheKey<Node> key(old_context, node);
    typename ContextCacheMap<Node>::iterator found = cache.find(key);
    assert (found != cache.end() && "Context pop not matching to last context push");
    info.set_context(old_context); // Set to the context that was on the stack
    info.context_stack.pop_back();
}

void IRCacheMutator::push_context(Expr expr) {
    expr_context_cache.push(context_info, expr);
}
void IRCacheMutator::push_context(Stmt stmt) {
    stmt_context_cache.push(context_info, stmt);
}

void IRCacheMutator::pop_context(Expr expr) {
    expr_context_cache.pop(context_info, expr);
}
void IRCacheMutator::pop_context(Stmt stmt) {
    stmt_context_cache.pop(context_info, stmt);
}

Expr IRCacheMutator::mutate(Expr e) {
    if (global_options.mutator_cache) {
        // First check to see whether there is a result in the cache.
        CacheKey<Expr> key(context(), e);
        typename CacheMap<Expr>::iterator found = expr_cache.find(key);
        if (found != expr_cache.end()) {
            global_statistics.mutator_cache_hits++;
            if (global_options.mutator_cache_check && 
                global_statistics.mutator_cache_hits + global_statistics.mutator_cache_misses + 
                global_statistics.mutator_cache_savings < global_options.mutator_cache_check_limit) {
                int misses = global_statistics.mutator_cache_misses;
                int hits = global_statistics.mutator_cache_hits;
                //std::cout << "Cache[" << context() << "] " << e << " --> " << found->second << "\n";
                // Check the cache for correctness by recomputing.
                Expr result = IRMutator::mutate(e);
                assert (equal(result, found->second) && "Cache error.  Do callers of IRCacheMutator contain push_context and pop_context?");
                // After checking, there will be additional misses.  These are the saved misses.
                // In addition, there may be additional hits which are also savings.
                global_statistics.mutator_cache_savings += global_statistics.mutator_cache_misses - misses;
                global_statistics.mutator_cache_misses = misses;
                global_statistics.mutator_cache_savings += global_statistics.mutator_cache_hits - hits;
                global_statistics.mutator_cache_hits = hits;
            }
            return found->second;
        } else {
            global_statistics.mutator_cache_misses++;
            //std::cout << "Mutate: " << e << "\n";
            Expr result = IRMutator::mutate(e);
            expr_cache[key] = result;
            return result;
        }
    } else {
        global_statistics.mutator_cache_misses++;
        return IRMutator::mutate(e);
    }
}

Stmt IRCacheMutator::mutate(Stmt s) {
    if (global_options.mutator_cache) {
        // First check to see whether there is a result in the cache.
        CacheKey<Stmt> key(context(), s);
        typename CacheMap<Stmt>::iterator found = stmt_cache.find(key);
        if (found != stmt_cache.end()) {
            global_statistics.mutator_cache_hits++;
            if (global_options.mutator_cache_check && 
                global_statistics.mutator_cache_hits + global_statistics.mutator_cache_misses + 
                global_statistics.mutator_cache_savings < global_options.mutator_cache_check_limit) {
                int misses = global_statistics.mutator_cache_misses;
                int hits = global_statistics.mutator_cache_hits;
                //std::cout << "Cache[" << context() << "] " << s << " --> " << found->second << "\n";
                // Check the cache for correctness by recomputing.
                Stmt result = IRMutator::mutate(s);
                assert (equal(result, found->second) && "Cache error.  Do callers of IRCacheMutator contain push_context and pop_context?");
                global_statistics.mutator_cache_savings += global_statistics.mutator_cache_misses - misses;
                global_statistics.mutator_cache_misses = misses;
                global_statistics.mutator_cache_savings += global_statistics.mutator_cache_hits - hits;
                global_statistics.mutator_cache_hits = hits;
            }
            return found->second;
        } else {
            global_statistics.mutator_cache_misses++;
            //std::cout << "Mutate: " << s << "\n";
            Stmt result = IRMutator::mutate(s);
            stmt_cache[key] = result;
            return result;
        }
    } else {
        global_statistics.mutator_cache_misses++;
        return IRMutator::mutate(s);
    }
}

// end namespace Internal
}
}
