#include "Context.h"
#include "IR.h"
#include "IRInspector.h"
#include "IROperator.h"
#include "IRPrinter.h"

#include <string>
#include <vector>
#include <utility>
#include <map>

namespace Halide { 
namespace Internal {

bool operator< (const NodeKey &a, const NodeKey &b) {
    if (a.context != b.context) return a.context < b.context;
    else return a.node.ptr < b.node.ptr;
}

namespace ContextInternal {

int ChildContext::lookup_define(int current_context, const IRHandle &node) {
    int context;
    assert (node.defined() && "Context push with undefined");
    NodeKey key(current_context, node); // Key to search for previous push result.
    typename ChildContext::iterator found = find(key); // Needs typename keyword?
    if (found == this->end()) {
        // Not found in the map. Create a new context and add it to the map.
        (*this)[key] = context = next_context++;
    } else {
        // Found in the map.  Use the map result.
        context = found->second;
    }
    return context;
}

// lookup method that does not define a new context.
// used only for verification when returning to parent context.
int ChildContext::lookup(int current_context, const IRHandle &node) {
    assert (node.defined() && "ChildContext lookup with undefined");
    NodeKey key(current_context, node); // Key to search for previous push result.
    typename ChildContext::iterator found = find(key); // Needs typename keyword?
    if (found == this->end()) {
        return 0;
    } else {
        // Found in the map.  Use the map result.
        return found->second;
    }
}
        

const DefiningNode *DefiningMap::lookup(int _context) const {
    typename DefiningMap::const_iterator found = find(_context);
    if (found == end()) {
        return NULL;
    } else {
        return &(found->second);
    }
}

// end namespace ContextInternal
}

ContextManager::ContextManager() : my_current_context(1) {
    clear();
}

void ContextManager::set_parent(int _context, int _parent) {
    while((int) (parent_vector.size()) <= _context) {
        parent_vector.push_back(0); // Default parent is 0, a non-context.
    }
    parent_vector[_context] = _parent;
    return;
}

int ContextManager::parent(int _context) const {
    if (_context <= 0 || _context >= (int) parent_vector.size()) {
        std::cerr << "Error: No parent for context " << _context << "\n";
        assert(0 && "No parent for context");
    }
    return parent_vector[_context];
}

void ContextManager::clear() {
    current_definition = DefiningNode();
    my_current_context = 1;
    child_context.clear();
    defining_map.clear();
    parent_vector.clear();
    set_parent(1, 0);
    defining_map.set(1, current_definition);
}

template<typename Node>
inline void ContextManager::push_node(Node node) { 
    // Ensure that the application programmer does not push the same context defining node
    // multiple times.  This could happen if there are multiple visitors managing context.
    // It would be a serious error.
    assert(! node.same_as(current_definition.node()) && "Invalid recursive push of same defining node");
    
    // Get the child context created by pushing to node in current context.
    int _parent = current_context();
    int _child = child_context.lookup_define(_parent, node);
    
    // Remember the current defining node for error checking and record it in the map.
    current_definition = DefiningNode(_parent, node);
    defining_map.set(_child, current_definition);
    
    // Remember the parent relationship between contexts.
    set_parent(_child, _parent);
    
    // Switch to the child context.
    my_current_context = _child; 
}

void ContextManager::push(Expr expr) {
    ContextManager::push_node(expr);
}

void ContextManager::push(Stmt stmt) {
    ContextManager::push_node(stmt);
}

void ContextManager::pop(IRHandle node) {
    // Look for the parent of the current context.
    int _parent = parent(current_context());
    assert(_parent != 0 && "Undefined parent of current context");
    
    // Verify that the node in the parent context actually maps to the current context.
    int _child = child_context.lookup(_parent, node);
    assert(_child == current_context() && "Context pop does not match push");
    
    // When the context is popped, we restore the current defining node so that
    // it again becomes valid to reenter the context that we have left.
    const DefiningNode *def_node = defining_map.lookup(_parent);
    if (! def_node) {
        std::cerr << "Popping from context " << _child << " to " << _parent << " failed\n";
        assert(def_node && "Cannot find defining node for popped context");
    }
    current_definition = *def_node;
    
    //std::cout << "[" << _parent << "] Popped from " << _child << "\n";
    
    // Switch to the parent context.
    my_current_context = _parent;
}

bool ContextManager::enter(IRHandle node) {
    int _child = child_context.lookup(current_context(), node);
    if (_child == current_context()) {
        std::cerr << "Error: Child context is the same as current context " << _child << "\n";
        assert(0 && "Child context the same as current context");
    }
    if (_child != 0) {
        // A child context is defined for this node in the current context.
        // Enter that context and return true.
        // Fetch the defining node as required for error checking.
        const DefiningNode *def_node = defining_map.lookup(_child);
        assert(def_node && "Cannot find defining node for child context");
        current_definition = *def_node;
        my_current_context = _child;
        return true;
    } else {
        return false;
    }
}

const DefiningNode *ContextManager::go(int context) {
    // Fetch the defining node for the target context.
    const DefiningNode *node = defining_node(context);
    assert(node && "Attempt to go to undefined context");
    my_current_context = context;
    current_definition = *node;
    return node;
}

}
}

