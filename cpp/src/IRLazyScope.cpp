#include "IRLazyScope.h"
#include "IR.h"
#include "IRPrinter.h"

#include <ostream>

// For the test code...
#include "IROperator.h"

namespace Halide { 
namespace Internal {

ContextManager IRLazyScope::context_mgr;
IRLazyScopeInternal::BindingMap IRLazyScope::variable_map;
IRLazyScopeInternal::BindingMap IRLazyScope::target_map;

void IRLazyScope::clear() {
    context_mgr.clear();
    variable_map.clear();
    target_map.clear();
    call_stack.clear();
}

void IRLazyScope::process(const Stmt& stmt) {
    std::cout << "[" << current_context() << "] IRLazyScope process:\n" << stmt << "\n";
    bool entered = fast_enter(stmt);
    Super::process(stmt);
    fast_leave(entered, stmt);
}

void IRLazyScope::process(const Expr& expr) {
    std::cout << "[" << current_context() << "] IRLazyScope process: " << expr << "\n";
    bool entered = fast_enter(expr);
    Super::process(expr);
    fast_leave(entered, expr);
}

bool IRLazyScope::fast_enter(const IRHandle &node) {
    //std::cout << "[" << current_context() << "] IRLazyScope call make_context\n";
    // First see whether a new context is required.
    int original_context = context_mgr.current_context();
    node.accept(&make_context);
    // Did we push into a new context?  If so, flag as entered.
    bool entered = context_mgr.current_context() != original_context;
    // If not entered already, try to enter a previously defined context.
    if (! entered) entered = context_mgr.enter(node);
    return entered;
}

void IRLazyScope::enter(const IRHandle &node) {
    int original_context = current_context();
    bool entered = fast_enter(node);
    call_stack.push_back(IRLazyScopeInternal::CallEnter(node, entered, original_context));
    return;
}

void IRLazyScope::leave(const IRHandle &node) {
    assert(call_stack.size() > 0 && "Attempt to leave with empty context call stack");
    IRLazyScopeInternal::CallEnter entry = call_stack.back();
    assert(! entry.was_call && "Leave after call");
    assert(entry.node.same_as(node) && "Mismatch of enter and leave");
    call_stack.pop_back();
    context_mgr.leave(entry.entered, node);
    assert(entry.return_context == current_context() && "leave did not restore the original context");
    return;
}

const DefiningNode *IRLazyScope::call(int context) {
    call_stack.push_back(IRLazyScopeInternal::CallEnter(context, current_context()));
    return context_mgr.go(context);
}

void IRLazyScope::ret(int context) {
    assert(call_stack.size() > 0 && "Attempt to ret with empty context call stack");
    IRLazyScopeInternal::CallEnter call = call_stack.back();
    assert(call.was_call && "Ret after enter");
    assert(call.context == context && "Mismatch of call and ret");
    call_stack.pop_back();
    context_mgr.go(call.return_context);
    return;
}

int IRLazyScope::lookup(IRLazyScopeInternal::BindingMap &map, std::string name) {
    int context = current_context();
    
    while (context != 0) {
        int result = map.lookup(context, name);
        if (result >= 0) {
            // A result value of 0 means unbound.  It can be cached in the map for efficiency.
            if (context != current_context()) {
                // The result was found in another context.  Add it to the current context.
                map.bind(current_context(), name, result);
            }
            return result;
        }
        context = context_mgr.parent(context);
    }
    // The variable was not bound. result is 0.
    // Add the unbound result to the map in the current context.
    map.bind(current_context(), name, 0); 
    return 0;
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
void MakeContext::visit(const Let *op) {
    //std::cout << "[" << context_mgr.current_context() << "] MakeContext Let:\n" << Expr(op);
    context_mgr.push(op);
    int defining_context = context_mgr.current_context();
    context_mgr.push(op->body);
    std::cout << "[" << context_mgr.current_context() << "] Bind " << op->name << " to " << defining_context << "\n";
    lazy_scope->bind(op->name, defining_context);
    context_mgr.pop(op->body);
}

void MakeContext::visit(const LetStmt *op) {
    //std::cout << "[" << context_mgr.current_context() << "] MakeContext LetStmt:\n" << Stmt(op);
    context_mgr.push(op);
    int defining_context = context_mgr.current_context();
    context_mgr.push(op->body);
    std::cout << "[" << context_mgr.current_context() << "] Bind " << op->name << " to " << defining_context << "\n";
    lazy_scope->bind(op->name, defining_context);
    context_mgr.pop(op->body);
}

void MakeContext::visit(const For *op) {
    //std::cout << "[" << context_mgr.current_context() << "] MakeContext For:\n" << Stmt(op);
    context_mgr.push(op);
    int defining_context = context_mgr.current_context();
    context_mgr.push(op->body);
    std::cout << "[" << context_mgr.current_context() << "] Bind " << op->name << " to " << defining_context << "\n";
    lazy_scope->bind(op->name, defining_context);
    context_mgr.pop(op->body);
}

// TargetVar and StmtTargetVar do not normally appear in the tree.
// When they do appear (as part of running the solver), they modify
// the interpretation of underlying Solve nodes so they create a new
// context.  Because they have no child nodes that are interpreted in
// the enclosing context, only one context is needed for the defining
// node itself.
void MakeContext::visit(const TargetVar *op) {
    context_mgr.push(op);
    lazy_scope->bind(op->name, context_mgr.current_context());
}

void MakeContext::visit(const StmtTargetVar *op) {
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
        // Return -1 for not found, because 0 means not bound and that can be stored in the map.
        return -1;
    } else {
        return found->second;
    }
}


} // end namespace IRLazyScopeInternal



namespace {
    // A simple tree walker runs over the tree.
    class Walker : public IRLazyScope {
    public:
        using IRLazyScope::visit;
        using IRLazyScope::process;
        
        //virtual void process(const Expr &expr) { std::cout << "Walker process " << expr << "\n"; IRLazyScope::process(expr); }
        
        // Walk the tree.
        virtual void visit(const Variable *op) {
            std::cout << "Walker visit Variable\n";
            // Look up to see whether the variable is defined.
            int found = find_variable(op->name);
            if (found) {
                std::cout << ">>> " << current_context() << " Found " << op->name << "[" << found << "]" << "\n";
                const DefiningNode *def = call(found);
                const For *fornode = def->node().as<For>();
                const Let *let = def->node().as<Let>();
                const LetStmt *letstmt = def->node().as<LetStmt>();
                if (fornode) {
                    std::cout << op->name << " bound to:\n" << Stmt(fornode);
                } else if (let) {
                    std::cout << op->name << " bound to:\n" << Expr(let);
                } else if (letstmt) {
                    std::cout << op->name << " bound to:\n" << Stmt(letstmt);
                }
                ret(found);
                std::cout << "[" << current_context() << "] Current context\n";
            } else {
                std::cout << "Could not find " << op->name << " in context " << current_context() << "\n";
            }
        }
    };
}   

/* Test the lazy scope. */
void lazy_scope_test() {
    // Build a program tree.
    Type i32 = Int(32);
    Expr x = new Variable(Int(32), "x");
    Expr y = new Variable(Int(32), "y");
    Expr a = new Variable(Int(32), "a");

    Expr input = new Call(Int(16), "input", vec((x - 10) % 100 + 10));
    Expr select = new Select(x > 3, new Select(x < 87, input, new Cast(Int(16), -17)),
                             new Cast(Int(16), -17));
    Stmt store = new Store("buf", select, x - 1);
    PartitionInfo partition(true);
    Stmt for_loop = new For("x", 0, 100, For::Parallel, partition, store);
    Stmt letstmt = new LetStmt("y", a * 2 + 5, for_loop);
    Expr call = new Call(i32, "buf", vec(max(min(x,100),0)));
    Expr call2 = new Call(i32, "buf", vec(max(min(x-1,100),0)));
    Expr call3 = new Call(i32, "buf", vec(Expr(new Clamp(Clamp::Reflect, x+1, 0, 100))));
    Stmt store2 = new Store("out", call + call2 + call3 + 1, x);
    PartitionInfo partition2(Interval(1,99));
    Stmt for_loop2 = new For("x", 0, 100, For::Serial, partition2, store2);
    Stmt pipeline = new Pipeline("buf", letstmt, Stmt(), for_loop2);
    
    Walker walk;
    
    std::cout << "Commence lazy scope test\n";
    walk.process(pipeline);
    std::cout << "Lazy scope test completed\n";
    
    x.accept(&walk);
}

}
}


