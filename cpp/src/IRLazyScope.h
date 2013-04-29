#ifndef HALIDE_IR_LAZYSCOPE_H
#define HALIDE_IR_LAZYSCOPE_H

#include "IRVisitor.h"
#include "IRInspector.h"
#include "Context.h"

#include <map>

/** \file
 * Defines the base class for things that recursively walk over the IR with lazy scope
 * binding of variables and context management. */

namespace Halide {
namespace Internal {

class IRLazyScope;

namespace IRLazyScopeInternal {

// A class that inspects a node and decides whether it needs to have a new context,
// and creates the required context(s). It leaves the context set inside the new
// context applied to the current node, even though it may also create a new context
// for a child node.
class MakeContext : public IRInspector {
    // process methods are not required.
    
    using IRInspector::visit;
    
    // Keep track of the context manager being used.
    ContextManager &context_mgr;
    
    // visit methods exist for the node types that can change the semantics
    // of other nodes that underly them.
    void visit(Let *op);
    void visit(LetStmt *op);
    void visit(For *op);
    void visit(TargetVar *op);
    void visit(StmtTargetVar *op);
    
    
public:
    MakeContext(ContextManager &mgr) : context_mgr(mgr) { }
    
    // Set the lazy_scope pointer so that this MakeContext class
    // can access the lazy_scope to bind variables etc.
    IRLazyScope *lazy_scope;
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

} // end namespace IRLazyScopeInternal

/** A base class for algorithms that need to recursively walk over the
 * IR while tracking contexts and lazy binding of variables.
 * Certain nodes initiate a new context; a variable is bound to the
 * node that defines the variable, in the enclosing context.
 * All the work in this class is done by the process method.
 * This ensures that the context manipulations and variable bindings
 * are handled before the visit methods of outer classes are invoked.
 */
class IRLazyScope : public IRVisitor {
    // Allow MakeContext access to private members.
    friend class IRLazyScopeInternal::MakeContext;
    
    // Simplify coding and simplify any change of the base class.
    typedef IRVisitor Super;

    // context_mgr is the context manager to be used by all classes that derive
    // from IRLazyScope.  It is shared among instances.
    static ContextManager context_mgr;
    
    // Bindings of variables and targets are shared among all class instances that derive
    // from IRLazyScope.  This is not multi-thread safe.
    static IRLazyScopeInternal::BindingMap variable_map; // Record the bindings of variables.
    static IRLazyScopeInternal::BindingMap target_map; // Record the TargetVar nodes encountered.
    
    /* Bind a variable by specifying the name and defining context.
     * The binding is recorded in the context in which the variable first becomes bound.
     * Note that if the same defining node appears at multiple points in the tree then it
     * must have the same children; and if it appears in the same context then it must 
     * have the same semantics; only then will it have the same bound context
     * for the variable.  For this reason, there is no need to ever remove bindings. */
    inline void bind(std::string name, int defining_context) { variable_map.bind(context_mgr.current_context(), name, defining_context); }
    
    /* Record a target by specifying the name and defining context. */
    inline void target(std::string name, int defining_context) { target_map.bind(context_mgr.current_context(), name, defining_context); }
    
    IRLazyScopeInternal::MakeContext make_context;
    
    // Methods to enter and leave context associated with a node;
    bool enter(const IRHandle &node);
    inline void leave(bool entered, const IRHandle &node) { context_mgr.leave(entered, node); }

public:
    IRLazyScope() : make_context(context_mgr) { make_context.lazy_scope = this; }
    
    // Reset the lazy scope.  Also resets the shared context manager.
    void reset();
    
    virtual void process(const Stmt &stmt);
    virtual void process(const Expr &expr);
    
    // IRLazyScope does not override any of the visit methods.
    // Such an override would be too late because a derived class
    // may have already executed its own visit method.
};

}
}

#endif
