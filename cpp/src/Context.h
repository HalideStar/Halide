#ifndef HALIDE_CONTEXT_H
#define HALIDE_CONTEXT_H

/** \file 
 * Defines a context manager.  Certain nodes change the semantics of underlying
 * nodes.  An obvious example is Let bindings which change the semantics of
 * certain Variable nodes.  The context manager is used to create a context
 * to reflect the changing semantics.  Information can then be cached
 * correctly using (context, node) as the key.  Also, variable bindings can
 * be stored indexed by the same key.  The context manager maintains a parent
 * relation between contexts.
 *
 * For this all to work correctly, an entire pass of tree walking must use
 * the same context manager to ensure that context maintenance is consistent
 * throughout the tree.  Contexts are based on the path from each node to
 * the root of the tree, as reflected in the nesting relationship between
 * contexts.  (As an exception, a tree walker may be called
 * that does not use the context manager, but that tree walker cannot call
 * back into a tree walker that uses the context manager).
 *
 * Complete generality is achieved by defining a new context for each node
 * in each context.  In practice, however, we define new contexts only for
 * nodes that can semantically modify the underlying tree.  These are:
 * Let, LetStmt, For, TargetVar, StmtTargetVar.  It is possible that
 * Assert could be used in a manner to provide semantic modification.
 */

#include "IR.h"

#include <string>
#include <vector>
//#include <utility>
#include <map>

# define TRACE_CONTEXT 0
# define CHECK_CONTEXT 1

namespace Halide { 
namespace Internal {

/** A little class for a node key.  A node key is used to build
 * a map that returns information specific to a particular node in a particular
 * context.  The key is  context x Node.  */
class NodeKey {
public:
    int context; // Current context unique ID
    IRHandle node; // Handle to parse tree node.  Ensures it cannot be deleted while it is cached.
    NodeKey(int _context, IRHandle _node) : context(_context), node(_node) {}
};

bool operator< (const NodeKey &a, const NodeKey &b);

/** A little class for a defining node.  It is similar to a NodeKey, but
 * unlike a NodeKey, it can be used to access the Expr or Stmt that was
 * the defining node. This is not used as a map key, but as map data. */
class DefiningNode {
    int my_context;
    bool my_is_expr; // True if Expr; false if Stmt.
    IRHandle my_node;
public:
    DefiningNode(int _context, Stmt _stmt) : my_context(_context), my_is_expr(false), my_node(_stmt) {}
    DefiningNode(int _context, Expr _expr) : my_context(_context), my_is_expr(true), my_node(_expr) {}
    
    // Constructor to allow instances to be created without initialisation.
    DefiningNode() : my_context(0), my_is_expr(false), my_node(Stmt()) {}
    
    bool is_expr() const { return my_is_expr; }
    int context() const { return my_context; }
    
    // node returns the node as a generic IRHandle.
    IRHandle node() const { return my_node; }
    
    // To obtain the Expr or Stmt, use the appropriate method.
    Expr expr() const { 
        assert(is_expr() && "expr applied to non-Expr defining node"); 
        return Expr((BaseExprNode *) my_node.ptr); 
    }
    
    Stmt stmt() const {
        assert(! is_expr() && "stmt applied to non-Stmt defining node"); 
        return Stmt((BaseStmtNode *) my_node.ptr); 
    }
    
    // To obtain Expr or Stmt by type conversion.
    operator Expr() const { return expr(); }
    operator Stmt() const { return stmt(); }
};


// Suppport classes are hidden in their own namespace
namespace ContextInternal {
 
/** Context  management. General concept of context, not specific to variable bindings at all. */

/** A small class for context child maps.  The context child map records the
 * context ID that is reached by pushing a particular node in a particular context.
 * This ensure that we get to the same context each time we visit the same node
 * in the same enclosing context. If no entry is found, it creates a new context ID. */
class ChildContext : public std::map<NodeKey, int> {
    int next_context;
public:
    // context 0 is invalid and 1 is the starting context, so the first new context is 2.
    // See ContextManager class definition.
    ChildContext() : next_context(2) { }
    
    // lookup_define method searches for map record of context to enter when
    // when pushing to node from current_context.  If not found, it creates and remembers
    // a new context.
    int lookup_define(int current_context, const IRHandle &node);
    
    // lookup methods returns 0 if the definition is not already found.  Use this for verification.
    int lookup(int current_context, const IRHandle &node);
    
    int context_count() { return next_context - 2; }
};

/** A little class to build a map that records the defining node and its enclosing context for each context.
 * The lookup method returns a pointer to the NodeKey if found; otherwise it returns NULL.
 * The set method records the defining node key for a given context. */
class DefiningMap : public std::map<int, DefiningNode> {
public: 
    const DefiningNode *lookup(int _context) const;
    inline void set(int _context, DefiningNode defining) { (*this)[_context] = defining; }
};

/** A little class for the keys to the bindings map. The context is the context in which the variable
 * is bound to the new value.  The name is, of course, the variable name string. */
class BindingKey {
public:
    int context; // Bound context unique ID
    std::string name; // Variable name
    BindingKey(int _context, std::string _name) : context(_context), name(_name) {}
};

bool operator< (const BindingKey &a, const BindingKey &b);

// The values of the binding maps are simply the context associated with the defining node. 

/** A class for the bindings map. This map translates context ID and variable name string
 * into context ID of the defining node. You can then go to that context and gain access to the
 * defining node. */
class BindingMap : public std::map<BindingKey, int> {
public:
    // Bind a name to a defining context.  The binding may already exist, in which case the rebinding is redundant.
    // There is no check whether the binding conflicts with an existing binding.
    void bind(int context, std::string name, int defining_context) { (*this)[BindingKey(context, name)] = defining_context; }
    
    // Look up a binding and return the defining context, or 0 if not defined in the specified context.
    int lookup(int context, std::string name) const;
};

// end namespace ContextInternal
}

/** A class to store contexts pushed by various defining nodes, and to implement push and pop of context. 
 * Only use one of these at a time.
 * The context Manager provides the following key services:
 *
 * current_context() returns the current context ID.
 * parent() looks up the parent of a given context, so you can search up through the contexts for information
 *      in your cache.
 * When you need to enter a new context for a child node, use push(Node) and then pop(Node) to leave the context.
 *      The context manager tracks and detects various errors.
 * When you need to go to a context and node (e.g. to process the value of a Let) use
 *      go(context) to go to the context of a particular node.  It returns the defining node object DefiningNode ptr.
 * node_key: Returns a NodeKey that you can use to cache/lookup information for a node in the current context.
 * defining_node: Returns a DefiningNode pointer (or NULL) that contains the defining node of the searched context.
 */
class ContextManager {
    // State variables.
    // ----------------
    
    // The ID of the current context.
    int my_current_context;
    
    // The current definition is held for error checking.
    // Checks ensure that the same node cannot be pushed twice in a row
    // (without an intervening pop).
    DefiningNode current_definition;
    
    // Count of user classes.
    int user_count;
    
    
    // Structures that record relationships.
    // -------------------------------------
   
    // Maps to record the child context.  context x Node --> context.
    ContextInternal::ChildContext child_context;
    
    // Map to record the node that defined each context.
    ContextInternal::DefiningMap defining_map;
    
    // Record of the parent of each context.
    std::vector<int> parent_vector;
    
    // Bindings of variables and targets.
    ContextInternal::BindingMap variable_map; // Record the bindings of variables.
    ContextInternal::BindingMap target_map; // Record the TargetVar nodes encountered.

    // Private methods.
    // ----------------
    
    // Set the parent context of a given child context.
    void set_parent(int child, int parent);
    
    // The template implementation of push - depends on whether node is Stmt or Expr
    template<typename Node> void push_node(Node node);
    
   
    /** Return the defining node (i.e. enclosing context and node itself) for a given context.
     * Returns NULL if not found, else pointer to defining node object.
     * Use this to convert a context ID to its defining node, e.g. as a way of implementing
     * variable binding by recording the context that is created by the node that defines
     * the variable so when you look up that context you can find the variable itself. */
    inline const DefiningNode *defining_node(int context) const { return defining_map.lookup(context); }
    
    /** Method to look for a variable in the current context in one of the maps.
     * The map is not a const argument because it can be updated for efficiency. */
    int lookup(ContextInternal::BindingMap &map, std::string name, int context);

public:
    // context 0 is invalid.  context 1 is the initial context.
    // See also ChildContext class definition.
    ContextManager();
    
    // Track the number of users.  When it returns to zero, clear the context information.
    // All classes using a shared context manager must employ add_user() and remove_user().
    void add_user();
    void remove_user();
    
    /** clear the context manager to commence a pass with all cached information discarded. */
    void clear();
    
    /** return the current context ID */
    inline int current_context() const { return my_current_context; }
    
    /** Return the parent context of a given context. */
    int parent(int context) const;
    
    /** Push and pop context by specifying the defining node.
     * If there is no context defined, a new context will be defined
     * for the defining node in the current context.
     * push enters the context for the defining node.
     * pop returns to the parent context. */
    // push has to know whether the node is Expr or Stmt to set up the DefiningNode object.
    // pop does not need to know.
    void push(Expr node);
    void push(Stmt node);
    void pop(IRHandle node);
    
    /** Enter the context for a node if one is defined.  
     * Returns boolean to indicate whether or not a context is entered. */
    bool enter(IRHandle node);
    /** Leave context if the entered flag indicates that context was entered. */
    void leave(bool entered, IRHandle node) { if (entered) pop(node); }
    
    /** Go to another context and return the DefiningNode of that context.
     * After this call, the context will be the context for working within the defining node.
     * However, it may be that you need to enter another context to work with a child of
     * that node. */
    const DefiningNode *go(int context);
    
    /** Return a node key in the current context.  Use this to cache information
     * relevant to the node in the current context. */
    inline NodeKey node_key(IRHandle node) const { return NodeKey(current_context(), node); }
    
    /** Bind a variable by specifying the name and defining context.
     * The binding should be recorded in the context in which the variable first becomes bound.
     * Note that if the same defining node appears at multiple points in the tree then it
     * must have the same children; and if it appears in the same context then it must 
     * have the same semantics; only then will it have the same bound context
     * for the variable.  For this reason, there is no need to ever remove bindings. */
    inline void bind(std::string name, int defining_context) { variable_map.bind(current_context(), name, defining_context); }
    
    /** Record a target by specifying the name and defining context. */
    inline void target(std::string name, int defining_context) { target_map.bind(current_context(), name, defining_context); }

    /** Search for a bound variable name, or a targetvar, in the current context or above. Returns the
     * context of the defining node. */
    int find_variable(std::string name) { return lookup(variable_map, name, current_context()); }
    int find_target(std::string name) { return lookup(target_map, name, current_context()); }
    
    /** Determine whether a given variable in the current context is one of the targets
     * in a specified search context. If the variable has been redefined below the search context,
     * then it is not a target. The search context here would be the internal context of a specific 
     * TargetVar or StmtTargetVar that we are solving. */
    bool is_target(std::string name, int search_context);
};

}
}

#endif
