#ifndef HALIDE_IR_CACHEMUTATOR_H
#define HALIDE_IR_CACHEMUTATOR_H

/** \file 
 * Defines a base class for passes over the IR that modify it.
 * This version of the mutator is built on IRLazyScope so
 * the derived class has access to variable bindings and contexts.
 * This class also caches mutation results: the same input node
 * in the same context is guaranteed to return the same mutation result,
 * so the cached value is returned.
 */

#include "IRLazyScope.h"
#include "IRMutator.h"
#include "IR.h"

#include <map>

namespace Halide { 
namespace Internal {

/** A little class for caching nodes devoid of their context. */
class CachedNode {
    bool my_is_expr; // True if Expr; false if Stmt.
    IRHandle my_node;
public:
    CachedNode(Stmt _stmt) : my_is_expr(false), my_node(_stmt) {}
    CachedNode(Expr _expr) : my_is_expr(true), my_node(_expr) {}
    
    // Constructor to allow instances to be created without initialisation.
    CachedNode() : my_is_expr(false), my_node(Stmt()) {}
    
    bool is_expr() const { return my_is_expr; }
    
    // Automatic conversion of CachedNode to correct node type
    operator Expr() const { 
        assert(is_expr() && "Expr applied to non-Expr node"); 
        return Expr((BaseExprNode *) my_node.ptr); 
    }
    
    operator Stmt() const {
        assert(! is_expr() && "Stmt applied to non-Stmt node"); 
        return Stmt((BaseStmtNode *) my_node.ptr); 
    }
};


/** A derived class that adds caching of mutation results.
 */
class IRCacheMutator : public IRLazyScope<IRMutator> {
    typedef IRLazyScope<IRMutator> Super;
    
public:

    /** This is the main interface for using a mutator. Also call
     * these in your subclass to mutate sub-expressions and
     * sub-statements.
     */
    Expr mutate(Expr expr);
    Stmt mutate(Stmt stmt);
    
    IRCacheMutator();

protected:

    // The cache of mutation results
    typedef std::map<NodeKey,CachedNode> CacheMap;
    CacheMap cache;
    
    template<typename Node>
    Node mutate(Node node, Node &result);
};    

}
}

#endif
