#ifndef HALIDE_IR_CACHEMUTATOR_H
#define HALIDE_IR_CACHEMUTATOR_H

/** \file 
 * Defines a base class for passes over the IR that modify it 
 */

#include "IRMutator.h"
#include "IR.h"

#include <vector>
#include <utility>
#include <map>

namespace Halide { 
namespace Internal {


/** A little template class for the keys to the cache map.  The same class can be used 
 * as the key to cache other information in other maps. 
 * In this implementation, Expr or Stmt objects are used as the cache key directly.
 * This means that the tree node cannot be deleted until the cache entry is deleted,
 * ensuring that the memory cannot be reused. */
template <typename Node>
class CacheKey {
public:
    int context; // Current context unique ID
    Node node; // Expr or Stmt of the node.  Ensures it cannot be deleted while it is cached.
    CacheKey(int _context, Node _node) : context(_context), node(_node) {}
};

template <typename Node>
bool operator< (const CacheKey<Node> &a, const CacheKey<Node> &b) {
    if (a.context != b.context) return a.context < b.context;
    else return a.node.ptr < b.node.ptr;
}

/** A template class for the mutator cache maps.  One each is used for Expr and Stmt caches. */
template <typename Node>
class CacheMap : public std::map<CacheKey<Node>, Node> {
};

/** A little class to represent context information. Tracks the current context ID and the
 * next ID to assign when pushing or popping context. Also holds the contexts that have been pushed. */
class ContextInfo {
    int my_context, next_context;
public:
    // stack of contexts pushed
    std::vector<int> context_stack;
    ContextInfo() { my_context = 1; next_context = 2; }
    inline int context() { return my_context; }
    inline int new_context() { my_context = next_context++; return my_context; }
    inline void set_context(int context) { my_context = context; }
};

/** A template class for context cache maps. The map caches context pushes so that the same node 
 * pushing in the same context results in the same new context.  */
template <typename Node>
class ContextCacheMap : public std::map<CacheKey<Node>, int> {
};

/** A template class for context stack management.  One each is used for Expr and Stmt contexts.
 * For example, to process a Let node, first process the value in the current
 * context, then push to a new context using the Let node as the node parameter. */
template <typename Node>
class ContextCache {
private:
    // cache of context pushes previously made, so that we can return the same context IDs
    ContextCacheMap<Node> cache;
    
public:
    void push(ContextInfo &info, Node node);
    void pop(ContextInfo &info, Node node);
};

/** A derived class that adds caching of mutation results, and context ID to IRMutator.
 */
class IRCacheMutator : public IRMutator {
public:

    /** Override the mutate interface so that caching can be implemented.
     */
    virtual Expr mutate(Expr expr);
    virtual Stmt mutate(Stmt stmt);
    
    IRCacheMutator() {}

protected:
    /** cache maps (context, source IRNode ID) to its mutated result. */
    CacheMap<Expr> expr_cache;
    CacheMap<Stmt> stmt_cache;
private:
    /* Context management.
     * context_info hoilds the current context and provides new context.
     * It also contains a stack of pushed and popped contexts, but this should
     * not be accessed directly: Use one of the ContextCaches: expr_context_cache 
     * or stmt_context_cache. */
    ContextInfo context_info;
    // ContextCache uses context_info.  Provides push and pop and also caches
    // previous pushes so that we can push into the same context.
    // There is a separate cache for Expr and Stmt nodes, but only one stack.
    ContextCache<Expr> expr_context_cache;
    ContextCache<Stmt> stmt_context_cache;
    
public:

    /** Call push_context with the node that defines the new context.
     * e.g. Let, LetStmt, TargetVar or StmtTargetVar.  Call push_context
     * immediately before processing the child node that needs the new
     * context, and call pop_context immediately after. */
    void push_context(Expr e);
    void push_context(Stmt s);
    void pop_context(Expr e);
    void pop_context(Stmt s);
    
    /** context() returns the current context ID. */
    inline int context() { return context_info.context(); }
    
    /** key() returns a cache key to access information relevant to a particular tree node.
     * If the node is a child, remember to push_context before creating the key, and
     * pop_context after. */
    inline CacheKey<Expr> key(Expr e) { return CacheKey<Expr>(context(), e); }
    inline CacheKey<Stmt> key(Stmt s) { return CacheKey<Stmt>(context(), s); }

    /** Let and TargetVar are the nodes that create context in the interpretation
     * of the tree.  Let creates a context for Variable, and TargetVar creates a
     * context for Solve.  Whatever contexts your visit methods create, they must
     * explicitly call push_context and pop_context.  Here is an example for Let.
     * 
    virtual void visit(const Let *op) {
        Expr value = mutate(op->value);
        context_push(op->body); // The body is mutated in a new context defined by the Let
        Expr body = mutate(op->body);
        context_pop(op->body);
        ...
    }
    */
    //virtual void visit(const LetStmt *);
    //virtual void visit(const TargetVar *);
    //virtual void visit(const StmtTargetVar *);
};    

}
}

#endif
