#ifndef HALIDE_LAZY_SCOPE_H
#define HALIDE_LAZY_SCOPE_H

/** \file 
 * Defines a lazy scope handler.  Rather than evaluate bindings as they
 * happen, the lazy scope handler records the binding for later processing.
 * When you look up the binding, you get the node that defines the binding.
 */

#include "IR.h"

#include <string>
#include <vector>
#include <utility>
#include <map>

namespace Halide { 
namespace Internal {

/** The lazy scope handler records bindings in a map.
 * The map is   name x scope_id --> defining_node x defining_scope_id
 * e.g. A LetStmt node N defines a variable "v".  The scope enclosing the
 * LetStmt node is S1.  The scope created by the node (and applying to
 * the body of the node) is S2.  
 * The bindings map records "v" x S2 --> N x S1.
 * A tree walker wishing to process the binding of the LetStmt
 * can use the map to get the node itself and the enclosing scope id.
 * It can then process the node as though it had arrived at it from the parent,
 * except that it cannot return a mutated value to the parent.  It can, however,
 * cache the mutated result.
 * 
 * When looking up a binding, the binding may not be found in the current
 * scope.  In that case, we look in the parent scope recursively until the binding
 * is found, or until there are no more scopes to try.
 *
 * Note: Because there are two types of nodes (Expr and Stmt), the bindings map
 * actually records both an Expr and a Stmt.  One of these is undefined.
 */

 namespace LazyScopeClasses {

/** A little class for the keys to the bindings map. */
class BindingKey {
public:
    int scope; // Current scope unique ID
    std::string name; // Variable name
    BindingKey(int _scope, std::string _name) : scope(_scope), name(_name) {}
};

bool operator< (const BindingKey &a, const BindingKey &b) {
    if (a.scope != b.scope) return a.scope < b.scope;
    else return a.name < b.name;
}

/** A little class for the values of the bindings map. */
class BindingValue {
public:
    int scope; // Scope ID that encloses the defining node.
    Expr expr; // If defining node is Expr, here it is.
    Stmt stmt; // If defining node is Stmt, here it is.
    BindingValue(int _scope, Expr _expr) : scope(_scope), expr(_expr), stmt(Stmt()) {}
    BindingValue(int _scope, Stmt _stmt) : scope(_scope), expr(Expr()), stmt(_stmt) {}
};

/** A class for the bindings map. */
class BindingMap : public std::map<BindingKey, BindingValue> {
};


//-------------------------------------------------------------------------------
/** Scope management. General concept of scope, not specific to variable bindings at all. */

/** A little template class for a node key.  A node key is used to build
 * a map that returns information specific to a particular node in a particular
 * scope.  The key is  scope x Node.  There will be two maps: one for Expr nodes and one
 * for Stmt nodes. Maps requires operator< so that is defined also.  */
template <typename Node>
class TNodeKey {
public:
    int scope; // Current scope unique ID
    Node node; // Expr or Stmt of the node.  Ensures it cannot be deleted while it is cached.
    TNodeKey(int _scope, Node _node) : scope(_scope), node(_node) {}
};

template <typename Node>
bool operator< (const TNodeKey<Node> &a, const TNodeKey<Node> &b) {
    if (a.scope != b.scope) return a.scope < b.scope;
    else return a.node.ptr < b.node.ptr;
}

/** A class to represent the current scope and the history of pushed scopes. Only use one of these at a time. */
class ScopeStackManager {
    int my_scope, next_scope;
    // stack of scopes that have been pushed
    std::vector<int> scope_stack;
    
public:
    // Scope 0 is invalid, so start in scope 1.
    ScopeStackManager() : my_scope(1), next_scope(2) { }
    
    // Return the current scope.
    inline int current_scope() { return my_scope; }
    
    // Return a brand new scope.  Only use this if the child scope is not found in the map.
    inline int new_scope() { my_scope = next_scope++; return my_scope; }
    
    // Push and pop a scope onto the scope stack.  The reason for the stack is
    // to verify that the scope transitions match up correctly, and to ensure pops do not exceed pushes.
    void push(int scope);
    void pop(int scope);
};

/** A template class for scope child maps.  The scope child map records the
 * scope ID that is reached by pushing a particular node in a particular scope.
 * This ensure that we get to the same scope each time we visit the same node
 * in the same context. */
template <typename Node>
class ChildScope : public std::map<TNodeKey<Node>, int> {
public:
    void push(ScopeStackManager &mgr, const Node &node);
    void pop(ScopeStackManager &mgr, const Node &node);
};

// end namespace LazyScopeClasses
}

/** A little class to use as a key for caching information about the current tree node.
 * Also use this class to record the node and scope that defines a scope. One of expr or stmt will be undefined. */
class NodeKey {
public:
    int scope;
    Expr expr;
    Stmt stmt;
    
    NodeKey(int _scope, const Expr &_expr) : scope(_scope), expr(_expr), stmt(Stmt()) {}
    NodeKey(int _scope, const Stmt &_stmt) : scope(_scope), expr(Expr()), stmt(_stmt) {}
    // Constructor required by STL.
    NodeKey() : scope(0), expr(Expr()), stmt(Stmt()) {}
};

namespace LazyScopeClasses {

/** A little class to build a map that records the defining node and its scope for each scope. */
class DefiningMap : public std::map<int, NodeKey> {
public: 
    const NodeKey *lookup(int _scope);
};

// end namesapce LazyScopeClasses
}

/** A class to store scopes pushed by various defining nodes, and to implement push and pop of scope. 
 * Only use one of these at a time.
 * The Scope Manager provides the following key services:
 *
 * current_scope() returns the current scope ID.
 * parent() looks up the parent of a given scope, so you can search up through the scopes for information
 *      in your cache.
 * When you need to enter a new scope, use push(Node) and then pop(Node) to leave the scope.
 *      The scope manager tracks and detects various errors.
 * node_key: Returns a NodeKey that you can use to cache/lookup information for a node in the current scope.
 */
class ScopeManager {
    ScopeStackManager stack_manager;
    
    // Maps to record the child scope.  scope x Node --> scope.
    ChildScope<Expr> expr_child_scope;
    ChildScope<Stmt> stmt_child_scope;
    
    // Map to record the node that defined each scope.
    DefiningMap defining_map;
    
    // The current definition is held for error checking.
    // Checks ensure that the same node cannot be pushed twice in a row
    // (without an intervening pop).
    NodeKey current_definition;
   
    // Record of the parent of each scope.
    std::vector<int> parent_vector;
    
    // Set the parent scope of a given scope.
    void set_parent(int scope, int parent);
    
public:
    inline int current_scope() { return stack_manager.current_scope(); }
    
    /** Return the parent scope of a given scope. */
    int parent(int scope);
    
    /** Push and pop scope by specifying the defining node. */
    void push(Expr expr);
    void push(Stmt stmt);
    void pop(Expr expr);
    void pop(Stmt stmt);
    
    /** Return a node key in the current scope */
    inline NodeKey node_key(Expr expr) { return NodeKey(current_scope(), expr); }
    inline NodeKey node_key(Stmt stmt) { return NodeKey(current_scope(), stmt); }
};

/** The main LazyScope class. */
class LazyScope {
private:
    // The scope manager is to be shared among any things that need the concept of scope.
    // This is important so that the scope IDs are unique throughout.
    ScopeManager &scope_manager;
    
public:
    LazyScope(ScopeManager &_scope_manager) : scope_manager(_scope_manager) {}
    
    /** Push and pop scope by specifying the defining node. */
    inline void push(Expr expr) { scope_manager.push(expr); }
    inline void push(Stmt stmt) { scope_manager.push(stmt); }
    inline void pop(Expr expr) { scope_manager.pop(expr); }
    inline void pop(Stmt stmt) { scope_manager.pop(stmt); }

# if 0
    /** Call to another scope location and return from that location. */
    void call(Expr expr);
    void call(Stmt stmt);
    void ret(Expr expr);
    void ret(Stmt stmt);
    
    // find returns a pointer to BindingValue or NULL.
    const BindingValue *find(int scope, std::string name);
# endif
};


}
}

#endif
