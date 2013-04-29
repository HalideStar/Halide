#include "IRLazyScope.h"
#include "IR.h"
#include "IRPrinter.h"
#include <ostream>

namespace Halide { 
namespace Internal {

ContextManager IRLazyScope::context_mgr;
IRLazyScopeInternal::BindingMap IRLazyScope::variable_map;
IRLazyScopeInternal::BindingMap IRLazyScope::target_map;

void IRLazyScope::reset() {
    context_mgr.reset();
    variable_map.clear();
    target_map.clear();
}

void IRLazyScope::process(const Stmt& stmt) {
    std::cout << "IRLazyScope process " << stmt << "\n";
    bool entered = enter(stmt);
    Super::process(stmt);
    leave(entered, stmt);
}

void IRLazyScope::process(const Expr& expr) {
    std::cout << "IRVisitor process " << expr << "\n";
    bool entered = enter(expr);
    Super::process(expr);
    leave(entered, expr);
}

bool IRLazyScope::enter(const IRHandle &node) {
    bool entered = context_mgr.enter(node);
    if (! entered) {
        // No existing context - see whether a new context is required.
        int original_context = context_mgr.current_context();
        node.accept(&make_context);
        // Did we push into a new context?  If so, flag as entered.
        entered |= context_mgr.current_context() != original_context;
    }
    return entered;
}

namespace IRLazyScopeInternal {
// Let, LetStmt and For nodes have one child
// where the bound variable has its bound value, and one or more other children where
// the bound variable is to be interpreted in the parent context.  For this reason,
// the body must be in a different context than the defining node.
// However, the defining node must also have a new context; if not then the body
// node could appear in the enclosing context and would be interpreted as entering
// a new context.  So, Let, LetStmt and For end up requiring two new contexts: one
// for the defining node itself and one for the body embedded within the defining node.
void MakeContext::visit(Let *op) {
    context_mgr.push(op);
    int defining_context = context_mgr.current_context();
    context_mgr.push(op->body);
    lazy_scope->bind(op->name, defining_context);
    context_mgr.pop(op->body);
}

void MakeContext::visit(LetStmt *op) {
    context_mgr.push(op);
    int defining_context = context_mgr.current_context();
    context_mgr.push(op->body);
    lazy_scope->bind(op->name, defining_context);
    context_mgr.pop(op->body);
}

void MakeContext::visit(For *op) {
    context_mgr.push(op);
    int defining_context = context_mgr.current_context();
    context_mgr.push(op->body);
    lazy_scope->bind(op->name, defining_context);
    context_mgr.pop(op->body);
}

// TargetVar and StmtTargetVar do not normally appear in the tree.
// When they do appear (as part of running the solver), they modify
// the interpretation of underlying Solve nodes so they create a new
// context.  Because they have no child nodes that are interpreted in
// the enclosing context, only one context is needed for the defining
// node itself.
void MakeContext::visit(TargetVar *op) {
    context_mgr.push(op);
    lazy_scope->bind(op->name, context_mgr.current_context());
}

void MakeContext::visit(StmtTargetVar *op) {
    context_mgr.push(op);
    lazy_scope->bind(op->name, context_mgr.current_context());
}


bool operator< (const BindingKey &a, const BindingKey &b) {
    if (a.context != b.context) return a.context < b.context;
    else return a.name < b.name;
}

int BindingMap::lookup(int context, std::string name) const {
    typename BindingMap::const_iterator found = BindingMap::find(BindingKey(context, name));
    if (found == end()) {
        return 0;
    } else {
        return found->second;
    }
}


} // end namespace IRLazyScopeInternal

}
}


