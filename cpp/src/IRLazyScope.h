#ifndef HALIDE_IR_LAZYSCOPE_H
#define HALIDE_IR_LAZYSCOPE_H

#include "IRProcess.h"
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
    void visit(const Let *op);
    void visit(const LetStmt *op);
    void visit(const For *op);
    void visit(const TargetVar *op);
    void visit(const StmtTargetVar *op);
    
    
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

/** A little struct for use by nested call and ret, and by nested enter and leave. */
struct CallEnter {
    bool was_call; // true if call; false if enter.
    int context; // context argument to the call.
    IRHandle node; // node provided in enter.
    bool entered; // remember whether nested context was entered or not.
    int return_context; // The context to return to.
    
    CallEnter(int _context, int _return) : was_call(true), context(_context), 
              node(IRHandle()), entered(false), return_context(_return) {}
    CallEnter(const IRHandle &_node, bool _entered, int _return) : was_call(false), context(0), 
              node(_node), entered(_entered), return_context(_return) {}
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
class IRLazyScope : public IRProcess {
    // Allow MakeContext access to private members.
    friend class IRLazyScopeInternal::MakeContext;
    
    // Simplify coding and simplify any change of the base class.
    typedef IRProcess Super;

    // context_mgr is the context manager to be used by all classes that derive
    // from IRLazyScope.  It is shared among instances.
    static ContextManager context_mgr;
    
    // Context call stack used by call and ret methods.
    std::vector<IRLazyScopeInternal::CallEnter> call_stack;
    
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
    
    // Method to look for a variable in the current context in one of the maps.
    // The map is not a const argument because it can be updated for efficiency.
    int lookup(IRLazyScopeInternal::BindingMap &map, std::string name);

    // Local class object used to detect nodes requiring new context(s).
    IRLazyScopeInternal::MakeContext make_context;
    
    // Methods to enter and leave context associated with a node;
    // These methods are faster than the ones that use the local stack.
    // Enter will create new context(s) if appropriate to the node type,
    // or enter an existing context if one is defined.
    // Leave will leave the context if entered says one was entered.
    // These methods are for internal use in this class, so do not employ a stack of state.
    bool fast_enter(const IRHandle &node);
    inline void fast_leave(bool entered, const IRHandle &node) { context_mgr.leave(entered, node); }

public:
    IRLazyScope() : make_context(context_mgr) { make_context.lazy_scope = this; }
    
    /** Clear the lazy scope.  Also resets the shared context manager. */
    void clear();
    
    virtual void process(const Stmt &stmt);
    virtual void process(const Expr &expr);
    
    // IRLazyScope does not override any of the visit methods.
    // Such an override would be too late because a derived class
    // may have already executed its own visit method.
    
    /** Return the current context as defined by the context manager. */
    inline int current_context() const { return context_mgr.current_context(); }

    /** Search for a bound variable name, or a targetvar, in the current context or above. Returns the
     * context of the defining node. */
    int find_variable(std::string name) { return lookup(variable_map, name); }
    int find_target(std::string name) { return lookup(target_map, name); }
    
    /** Call a context: Go to the context and return the defining node.
     * A stack is maintained locally. */
    const DefiningNode *call(int context); 
    /** Return to the context from which a call was issued. */
    void ret(int context);
    
    /** Enter and leave a context: In order to explicitly visit a child node,
     * you must enter the context if there is one, and leave afterwards.
     * These methods use the local stack. */
    void enter(const IRHandle &node);
    void leave(const IRHandle &node);

    /** Return a node key in the current context.  Use this to cache information
     * relevant to the node in the current context. */
    inline NodeKey node_key(IRHandle node) const { return NodeKey(current_context(), node); }
};


void lazy_scope_test();


}
}

#endif
