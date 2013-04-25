#include "LazyScope.h"
#include "IR.h"

#include <string>
#include <vector>
#include <utility>
#include <map>

namespace Halide { 
namespace Internal {


void ScopeStackManager::push(int scope) {
    scope_stack.push_back(my_scope);
    my_scope = scope;
}

void ScopeStackManager::pop(int scope) {
    assert(my_scope == scope && "Pop of incorrect scope");
    assert(! scope_stack.empty() && "Empty scope stack");
    my_scope = scope_stack.back();
    scope_stack.pop_back();
}


template<typename Node>
void ChildScope<Node>::push(ScopeStackManager &mgr, const Node &node) {
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
void ChildScope<Node>::pop(ScopeStackManager &mgr, const Node &node) {
    mgr.pop(mgr.current_scope()); // Defeats the check on the stack
    int scope = mgr.current_scope();
    // Check that the node that we are popping was indeed the one that was pushed.
    TNodeKey<Node> key(scope, node);
    typename ChildScope<Node>::iterator found = find(key);
    assert (found != this->end() && "Context pop not matching to last context push");
}


const NodeKey *DefiningMap::lookup(int _scope) {
    typename DefiningMap::iterator found = find(_scope);
    if (found == end()) {
        return NULL;
    } else {
        return &(found->second);
    }
}

void ScopeManager::set_parent(int _scope, int _parent) {
    while((int) (parent_vector.size()) <= _scope) {
        parent_vector.push_back(0); // Default parent is 0, a non-scope.
    }
    parent_vector[_scope] = _parent;
    return;
}

int ScopeManager::parent(int _scope) {
    assert(_scope > 0 && _scope < (int) parent_vector.size() && "No parent for scope.");
    return parent_vector[_scope];
}

void ScopeManager::push(Expr expr) { 
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

void ScopeManager::push(Stmt stmt) { 
    assert(! stmt.same_as(current_definition.stmt) && "Invalid recursive push of same defining node");
    int _parent = current_scope();
    stmt_child_scope.push(stack_manager, stmt); 
    defining_map[current_scope()] = current_definition = NodeKey(_parent, stmt);
    set_parent(current_scope(), _parent);
}

void ScopeManager::pop(Expr expr) {
    expr_child_scope.pop(stack_manager, expr);
    // When the scope is popped, we restore the current defining node so that
    // it again becomes valid to reenter the scope that we have left.
    const NodeKey *node = defining_map.lookup(current_scope());
    assert(node && "Cannot find defining node for popped scope");
    current_definition = *node;
}

void ScopeManager::pop(Stmt stmt) {
    stmt_child_scope.pop(stack_manager, stmt);
    const NodeKey *node = defining_map.lookup(current_scope());
    assert(node && "Cannot find defining node for popped scope");
    current_definition = *node;
}


# if 0    
const BindingValue * LazyScope::find(int scope, std::string name) {
    typename BindingMap::iterator iter = binding_map.find(BindingKey(scope, name));
    while (iter == binding_map.end() && scope != 0) {
        scope = parent(scope);
        assert(scope >= 0 && "Invalid scope returned by parent.");
        iter = binding_map.find(BindingKey(scope, name));
    }
    if (scope == 0) {
        return NULL;
    } else {
        // Found in a parent scope.  Record into the current scope also.
        binding_map[key] = iter->second;
        return &iter->second;
    }
}
# endif
}
}

