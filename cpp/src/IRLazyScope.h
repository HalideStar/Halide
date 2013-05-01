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

class IRLazyScopeBase;

namespace IRLazyScopeInternal {

/** A class that inspects a node and decides whether it needs to have a new context,
 * then creates the required context(s). It leaves the current context inside the new
 * context applied to the current node, even though it may also create a new context
 * for a child node. */
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
    IRLazyScopeBase *lazy_scope;
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
class IRLazyScopeBase {
    // Allow MakeContext access to private members.
    friend class IRLazyScopeInternal::MakeContext;

    // Local class object used to detect nodes requiring new context(s).
    IRLazyScopeInternal::MakeContext make_context;

    // Simplify coding and simplify any change of the base class.
    typedef IRProcess Super;

    // context_mgr is the context manager to be used by all classes that derive
    // from IRLazyScope.  It is shared among instances.
    static ContextManager context_mgr;
    
    // Context call stack used by call and ret methods.
    std::vector<IRLazyScopeInternal::CallEnter> call_stack;

protected:
    // Methods to enter and leave context associated with a node;
    // These methods are faster than the ones that use the local stack.
    // Enter will create new context(s) if appropriate to the node type,
    // or enter an existing context if one is defined.
    // Leave will leave the context if entered says one was entered.
    // These methods are for internal use in the LazyScope template class, so do not
    // employ the stack.
    bool fast_enter(const IRHandle &node);
    void fast_leave(bool entered, const IRHandle &node);

public:
    IRLazyScopeBase() : make_context(context_mgr) { make_context.lazy_scope = this; context_mgr.add_user(); }
    ~IRLazyScopeBase() { context_mgr.remove_user(); }
    
    /** Clear the lazy scope.  Also resets the shared context manager. */
    void clear();
    
    // IRLazyScope does not override any of the visit methods.
    // Such an override would be too late because a derived class
    // may have already executed its own visit method.
    
    /** Return the current context as defined by the context manager. */
    inline int current_context() const { return context_mgr.current_context(); }

    /** Search for a bound variable name, or a targetvar, in the current context or above. Returns the
     * context of the defining node. */
    int find_variable(std::string name) { return context_mgr.find_variable(name); }
    int find_target(std::string name) { return context_mgr.find_target(name); }
    
    /** Determine whether a given variable in the current context is one of the targets
     * in a specified search context. If the variable has been redefined below the search context,
     * then it is not a target. */
    bool is_target(std::string name, int search_context) { return context_mgr.is_target(name, search_context); }
    
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


template<typename IRSuper>
class IRLazyScope : public IRSuper, public IRLazyScopeBase {
public:

    virtual void process(const Stmt &stmt) {
        //std::cout << "[" << current_context() << "] IRLazyScope process:\n" << stmt << "\n";
        bool entered = fast_enter(stmt);
        IRSuper::process(stmt);
        fast_leave(entered, stmt);
    }

    virtual void process(const Expr& expr) {
        //std::cout << "[" << current_context() << "] IRLazyScope process: " << expr << "\n";
        //std::cout << "IRLazyScope::process(" << expr.ptr << ") in " << current_context() << "\n";
        bool entered = fast_enter(expr);
        IRSuper::process(expr);
        fast_leave(entered, expr);
        //std::cout << "IRLazyScope done with " << expr.ptr << " in " << current_context() << "\n";
    }
};

class IRLazyScopeProcess : public IRLazyScope<IRProcess> {
};

void lazy_scope_test();


}
}

#endif
