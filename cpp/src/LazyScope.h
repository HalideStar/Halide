#ifndef HALIDE_LAZY_SCOPE_H
#define HALIDE_LAZY_SCOPE_H

/** \file 
 * Defines a lazy scope handler.  Rather than evaluate bindings as they
 * happen, the lazy scope handler records the binding for later processing.
 * When you look up the binding, you get the node that defines the binding.
 */

#include "IR.h"
#include "IRInspector.h"

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

 // There are quite a few support classes that are used within LazyScope.
 // These are hidden in their own namespace.
 namespace LSInternal {


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

// end namespace LSInternal
}


/** A little class to use as a key for caching information about the current tree node.
 * Also use this class to record a node and its enclosing scope. One of expr or stmt will be undefined. */
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

typedef NodeKey ScopedNode;


namespace LSInternal {

/** A little class to build a map that records the defining node and its enclosing scope for each scope. */
class DefiningMap : public std::map<int, ScopedNode> {
public: 
    const ScopedNode *lookup(int _scope);
};

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
    LSInternal::ScopeStackManager stack_manager;
    
    // Maps to record the child scope.  scope x Node --> scope.
    LSInternal::ChildScope<Expr> expr_child_scope;
    LSInternal::ChildScope<Stmt> stmt_child_scope;
    
    // Map to record the node that defined each scope.
    LSInternal::DefiningMap defining_map;
    
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

// The values of the binding maps are scoped nodes. 

/** A class for the bindings map. */
class BindingMap : public std::map<BindingKey, ScopedNode> {
public:
    // Bind a name to a defining node.  The binding may already exist, in which case the rebinding is redundant.
    // There is no check whether the binding conflicts with an existing binding.
    void bind(int scope, std::string name, Expr expr) { (*this)[BindingKey(scope, name)] = ScopedNode(scope, expr); }
    void bind(int scope, std::string name, Stmt stmt) { (*this)[BindingKey(scope, name)] = ScopedNode(scope, stmt); }
    
    // Look up a binding and return the ScopedNode object or NULL if not found.
    const ScopedNode *lookup(int scope, std::string name) const;
};

// end namespace LSInternal
}

/** The main LazyScope class.  This class performs lazy bindings of variables
 * in Let, LetStmt and For nodes.  It also records target variables encountered
 * in TargetVar and StmtTargetVar nodes. */
class LazyScope {
private:
    // The scope manager is to be shared among any things that need the concept of scope.
    // This is important so that the scope IDs are unique throughout.
    LSInternal::ScopeManager scope_manager;
    
    LSInternal::BindingMap variable_map; // Record the bindings of variables.
    LSInternal::BindingMap target_map; // Record the TargetVar nodes encountered.
    
    // lookup returns a pointer to ScopedNode or NULL if not found.
    const ScopedNode *lookup(const LSInternal::BindingMap &map, int scope, std::string name);

public:
    /** Return the current scope ID */
    inline int current_scope() { return scope_manager.current_scope(); }
    
    /** Return the parent scope of a given scope. */
    inline int parent(int scope) { return scope_manager.parent(scope); }
    
    /** Return a node key in the current scope */
    inline NodeKey node_key(Expr expr) { return NodeKey(current_scope(), expr); }
    inline NodeKey node_key(Stmt stmt) { return NodeKey(current_scope(), stmt); }
    
    /** Push scope by specifying the defining node. */
    inline void push(Expr expr) { scope_manager.push(expr); }
    inline void push(Stmt stmt) { scope_manager.push(stmt); }
    
    /** Pop scope by specifying the defining node. */
    inline void pop(Expr expr) { scope_manager.pop(expr); }
    inline void pop(Stmt stmt) { scope_manager.pop(stmt); }
    
    /** Bind a variable by specifying the name and defining node.
     * The binding must be recorded in the parent scope, which is the scope of the defining node.
     * Note that if the same defining node appears at multiple points in the tree then it
     * must have the same children; and if it appears in the same scope then it must 
     * have the same semantics.  For this reason, there is no need to remove bindings. */
    inline void bind(std::string name, Expr expr) { variable_map.bind(current_scope(), name, expr); }
    inline void bind(std::string name, Stmt stmt) { variable_map.bind(current_scope(), name, stmt); }
    
    /** Record a target by specifying the name and defining node. */
    inline void target(std::string name, Expr expr) { target_map.bind(current_scope(), name, expr); }
    inline void target(std::string name, Stmt stmt) { target_map.bind(current_scope(), name, stmt); }
    
    /** Search for a bound variable name, or a targetvar, in the enclosing scope or above.
     * Note that bindings in the current scope are not relevant as they would be bindings defined
     * at other nodes within the current scope, not defined above the current node. */
    inline const ScopedNode *find_variable(std::string name) { return lookup(variable_map, current_scope(), name); }
    inline const ScopedNode *find_target(std::string name) { return lookup(target_map, current_scope(), name); }

# if 0
    /** Call to another scope location and return from that location. */
    void call(Expr expr);
    void call(Stmt stmt);
    void ret(Expr expr);
    void ret(Stmt stmt);
# endif
};

namespace LSInternal {

class LazyScopePreBinder : public IRInspector {
    Expr expr_child;
    Stmt stmt_child;
    
    LazyScope& lazy_scope;
    
public:
    LazyScopePreBinder(LazyScope& _lazy_scope) : lazy_scope(_lazy_scope) {};
    
    virtual void process(const Stmt& parent, const Stmt& child) {
        stmt_child = child;
        parent.accept(this);
    }
    
    virtual void process(const Stmt& parent, const Expr& child) {
        expr_child = child;
        parent.accept(this);
    }
    
    virtual void process(const Expr& parent, const Expr& child) {
        expr_child = child;
        parent.accept(this);
    }
    
protected:
    using IRInspector::visit;

    // Visit a Let node.  Enter a new scope if accessing the body.
    virtual void visit(Let *op) {
        // Create the binding of this Let node if it does not exist.
        lazy_scope.bind(op->name, op);
        if (op->body.same_as(expr_child)) {
            // The child node is the body.  It is visited in a new scope.
            lazy_scope.push(op);
        }
    }
    
    virtual void visit(LetStmt *op) {
        lazy_scope.bind(op->name, op);
        if (op->body.same_as(expr_child)) {
            lazy_scope.push(op);
        }
    }
    
    virtual void visit(For *op) {
        lazy_scope.bind(op->name, op);
        if (op->body.same_as(stmt_child)) {
            lazy_scope.push(op);
        }
    }
    
    virtual void visit(StmtTargetVar *op) {
        lazy_scope.target(op->var, op);
        lazy_scope.push(op);
    }
    
    virtual void visit(TargetVar *op) {
        lazy_scope.target(op->var, op);
        lazy_scope.push(op);
    }
};

class LazyScopePostBinder : public IRInspector {
    Expr expr_child;
    Stmt stmt_child;
    
    LazyScope& lazy_scope;
    
public:
    LazyScopePostBinder(LazyScope& _lazy_scope) : lazy_scope(_lazy_scope) {};
    
    virtual void process(const Stmt& parent, const Stmt& child) {
        stmt_child = child;
        parent.accept(this);
    }
    
    virtual void process(const Stmt& parent, const Expr& child) {
        expr_child = child;
        parent.accept(this);
    }
    
    virtual void process(const Expr& parent, const Expr& child) {
        expr_child = child;
        parent.accept(this);
    }
    
protected:
    using IRInspector::visit;

    // Visit a Let node.  Enter a new scope if accessing the body.
    virtual void visit(Let *op) {
        if (op->body.same_as(expr_child)) {
            // The child node is the body.  It is visited in a new scope.
            lazy_scope.pop(op);
        }
    }
    
    virtual void visit(LetStmt *op) {
        if (op->body.same_as(expr_child)) {
            lazy_scope.pop(op);
        }
    }
    
    virtual void visit(For *op) {
        if (op->body.same_as(stmt_child)) {
            lazy_scope.pop(op);
        }
    }
    
    virtual void visit(StmtTargetVar *op) {
        lazy_scope.pop(op);
    }
    
    virtual void visit(TargetVar *op) {
        lazy_scope.pop(op);
    }
};

// end namespace LSInternal
}

/** A class for capturing nodes that bind variables and set up
 * scopes.  There can be many such templates; each will use its own
 * LazyScope-like class.  A pass over the tree must consistently used only
 * one LazyScope-like class, and its corresponding binding class must be used
 * universally.
 * This template is a wrapper: wrap it around other tree walkers to capture
 * bindings into the LazyScope. 
 * e.g. trivially:  
 *     LazyScope
 *     LazyScopeBinder<IRVisitor> binder;*/
template<typename Wrapped>
class LazyScopeBinder : public Wrapped {
    // The prebinder gets called before the wrapped class's process method,
    // and the postbinder gets called after the wrapped class's process method.
    LSInternal::LazyScopePreBinder pre_binder;
    LSInternal::LazyScopePostBinder post_binder;
    
public:
    LazyScopeBinder(LazyScope &lazy_scope) : pre_binder(lazy_scope), post_binder(lazy_scope) {}
    
protected:
    virtual void process(const Stmt& parent, const Stmt& child) {
        pre_binder.process(parent, child);
        Wrapped::process(parent, child);
        post_binder.process(parent, child);
    }
    
    virtual void process(const Stmt& parent, const Expr& child) {
        pre_binder.process(parent, child);
        Wrapped::process(parent, child);
        post_binder.process(parent, child);
    }
    
    virtual void process(const Expr& parent, const Expr& child) {
        pre_binder.process(parent, child);
        Wrapped::process(parent, child);
        post_binder.process(parent, child);
    }
    
};


}
}

#endif
