#include "LazyScope.h"
#include "IR.h"
#include "IRInspector.h"

#include <string>
#include <vector>
#include <utility>
#include <map>

namespace Halide { 
namespace Internal {


void LSInternal::ScopeStackManager::push(int scope) {
    scope_stack.push_back(my_scope);
    my_scope = scope;
}

void LSInternal::ScopeStackManager::pop(int scope) {
    assert(my_scope == scope && "Pop of incorrect scope");
    assert(! scope_stack.empty() && "Empty scope stack");
    my_scope = scope_stack.back();
    scope_stack.pop_back();
}


template<typename Node>
void LSInternal::ChildScope<Node>::push(ScopeStackManager &mgr, const Node &node) {
    int scope;
    assert (node.defined() && "Scope push with undefined");
    TNodeKey<Node> key(mgr.current_scope(), node); // Key to search for previous push result.
    typename ChildScope<Node>::iterator found = find(key); // Need typename keyword
    if (found == this->end()) {
        // Not found in the map. Create a new scope and add it to the map.
        (*this)[key] = scope = mgr.new_scope();
    } else {
        // Found in the map.  Use the map result.
        scope = found->second;
    }
    mgr.push(scope);
}
        
template <typename Node>
void LSInternal::ChildScope<Node>::pop(ScopeStackManager &mgr, const Node &node) {
    mgr.pop(mgr.current_scope()); // Defeats the check on the stack
    int scope = mgr.current_scope();
    // Check that the node that we are popping was indeed the one that was pushed.
    TNodeKey<Node> key(scope, node);
    typename ChildScope<Node>::iterator found = find(key);
    assert (found != this->end() && "Context pop not matching to last context push");
}


const NodeKey *LSInternal::DefiningMap::lookup(int _scope) {
    typename DefiningMap::iterator found = find(_scope);
    if (found == end()) {
        return NULL;
    } else {
        return &(found->second);
    }
}

void LSInternal::ScopeManager::set_parent(int _scope, int _parent) {
    while((int) (parent_vector.size()) <= _scope) {
        parent_vector.push_back(0); // Default parent is 0, a non-scope.
    }
    parent_vector[_scope] = _parent;
    return;
}

int LSInternal::ScopeManager::parent(int _scope) {
    assert(_scope > 0 && _scope < (int) parent_vector.size() && "No parent for scope.");
    return parent_vector[_scope];
}

void LSInternal::ScopeManager::push(Expr expr) { 
    // Ensure that the application programmer does not push the same scope defining node
    // multiple times.  This could happen if there are multiple visitors managing scope.
    // It would be a serious error.
    assert(! expr.same_as(current_definition.expr) && "Invalid recursive push of same defining node");
    int _parent = current_scope();
    expr_child_scope.push(stack_manager, expr); 
    // Remember the current defining node for error checking.
    defining_map[current_scope()] = current_definition = NodeKey(_parent, expr);
    set_parent(current_scope(), _parent);
}

void LSInternal::ScopeManager::push(Stmt stmt) { 
    assert(! stmt.same_as(current_definition.stmt) && "Invalid recursive push of same defining node");
    int _parent = current_scope();
    stmt_child_scope.push(stack_manager, stmt); 
    defining_map[current_scope()] = current_definition = NodeKey(_parent, stmt);
    set_parent(current_scope(), _parent);
}

void LSInternal::ScopeManager::pop(Expr expr) {
    expr_child_scope.pop(stack_manager, expr);
    // When the scope is popped, we restore the current defining node so that
    // it again becomes valid to reenter the scope that we have left.
    const NodeKey *node = defining_map.lookup(current_scope());
    assert(node && "Cannot find defining node for popped scope");
    current_definition = *node;
}

void LSInternal::ScopeManager::pop(Stmt stmt) {
    stmt_child_scope.pop(stack_manager, stmt);
    const NodeKey *node = defining_map.lookup(current_scope());
    assert(node && "Cannot find defining node for popped scope");
    current_definition = *node;
}

const ScopedNode *LSInternal::BindingMap::lookup(int scope, std::string name) const {
    typename LSInternal::BindingMap::const_iterator found = BindingMap::find(LSInternal::BindingKey(scope, name));
    if (found == end()) {
        return NULL;
    } else {
        return &(found->second);
    }
}

// lookup a binding in a map. An efficiency trade-off exists whereby we could reduce calls of
// map.lookup by adding a reference to the found name in the final scope (or even in the intervening scopes)
// but on the other hand that makes the map larger and each lookup takes more time.
const ScopedNode *LazyScope::lookup(const LSInternal::BindingMap& map, int scope, std::string name) {
    // Do not look up variables defined in the current scope, because the definitions
    // are recorded in the scope of the defining node, not the scope of application of the
    // definition.
    if (scope != 0)
        scope = parent(scope);
    while (scope != 0) {
        const ScopedNode *found = map.lookup(scope, name);
        if (found)
            return found;
        scope = parent(scope);
    }
    return NULL;
}

LazyScope test_scope;
LazyScopeBinder<IRVisitor> test_global_binder(test_scope);

}
}

