#include "IRLazyScope.h"
#include "IR.h"
#include "IRPrinter.h"

#include <ostream>

// For the test code...
#include "IROperator.h"

// Set to true if nodes can create a context for their body node.
// If set to false, there is an assumption that a Let, LetStmt or For
// node cannot use the variable that it is defining within the definition.
# define BODY_CONTEXT 1

namespace Halide { 
namespace Internal {

ContextManager IRLazyScopeBase::context_mgr;

void IRLazyScopeBase::clear() {
    context_mgr.clear();
    //call_stack.clear();
}

bool IRLazyScopeBase::fast_enter(const IRHandle &node) {
    //std::cout << "[" << current_context() << "] IRLazyScopeBase call make_context\n";
    // First see whether a new context is required.
    int original_context = context_mgr.current_context();
    node.accept(&make_context);
    // Did we push into a new context?  If so, flag as entered.
    bool entered = context_mgr.current_context() != original_context;
    
#   if BODY_CONTEXT
        // If not entered already, try to enter a previously defined context.
        if (! entered) entered = context_mgr.enter(node);
#   endif

    //if (entered) std::cout << "fast_enter " << original_context << " --> " << current_context() << " by " << node.ptr << "\n";
    return entered;
}

void IRLazyScopeBase::fast_leave(bool entered, const IRHandle &node) { 
    //if (entered) std::cout << "fast leave " << current_context() << " by " << node.ptr << "\n";
    context_mgr.leave(entered, node); }

void IRLazyScopeBase::enter(const IRHandle &node) {
    int original_context = current_context();
    bool entered = fast_enter(node);
    call_stack.push_back(IRLazyScopeInternal::CallEnter(node, entered, original_context));
    return;
}

void IRLazyScopeBase::leave(const IRHandle &node) {
    assert(call_stack.size() > 0 && "Attempt to leave with empty context call stack");
    IRLazyScopeInternal::CallEnter entry = call_stack.back();
    assert(! entry.was_call && "Leave after call");
    assert(entry.node.same_as(node) && "Mismatch of enter and leave");
    call_stack.pop_back();
    context_mgr.leave(entry.entered, node);
    assert(entry.return_context == current_context() && "leave did not restore the original context");
    return;
}

const DefiningNode *IRLazyScopeBase::call(int context) {
    //assert(context != current_context() && "Call to current context");
    call_stack.push_back(IRLazyScopeInternal::CallEnter(context, current_context()));
# if TRACE_CONTEXT
    std::cout << "Call " << context << " from " << current_context() << "\n";
# endif
    return context_mgr.go(context);
}

void IRLazyScopeBase::ret(int context) {
    assert(call_stack.size() > 0 && "Attempt to ret with empty context call stack");
    IRLazyScopeInternal::CallEnter call = call_stack.back();
    assert(call.was_call && "Ret after enter");
    assert(call.context == context && "Mismatch of call and ret");
    call_stack.pop_back();
# if TRACE_CONTEXT
    std::cout << "Ret " << call.return_context << " from " << current_context() << "\n";
# endif
    context_mgr.go(call.return_context);
    return;
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
#   if BODY_CONTEXT
        context_mgr.push(op->body);
#   endif
    //std::cout << "[" << context_mgr.current_context() << "] Bind " << op->name << " to " << defining_context << "\n";
    context_mgr.bind(op->name, defining_context);
    context_mgr.target(op->name, 0);
#   if BODY_CONTEXT
        context_mgr.pop(op->body);
#   endif
}

void MakeContext::visit(const LetStmt *op) {
    //std::cout << "[" << context_mgr.current_context() << "] MakeContext LetStmt:\n" << Stmt(op);
    context_mgr.push(op);
    int defining_context = context_mgr.current_context();
#   if BODY_CONTEXT
        context_mgr.push(op->body);
#   endif
    //std::cout << "[" << context_mgr.current_context() << "] Bind " << op->name << " to " << defining_context << "\n";
    context_mgr.bind(op->name, defining_context);
    context_mgr.target(op->name, 0);
#   if BODY_CONTEXT
        context_mgr.pop(op->body);
#   endif
}

void MakeContext::visit(const For *op) {
    //std::cout << "[" << context_mgr.current_context() << "] MakeContext For:\n" << Stmt(op);
    context_mgr.push(op);
    int defining_context = context_mgr.current_context();
#   if BODY_CONTEXT
        context_mgr.push(op->body);
#   endif
    //std::cout << "[" << context_mgr.current_context() << "] Bind " << op->name << " to " << defining_context << "\n";
    context_mgr.bind(op->name, defining_context);
    context_mgr.target(op->name, 0); // Mark this as not being a target.
#   if BODY_CONTEXT
        context_mgr.pop(op->body);
#   endif
}

// TargetVar and StmtTargetVar do not normally appear in the tree.
// When they do appear (as part of running the solver), they modify
// the interpretation of underlying Solve nodes so they create a new
// context.  Because they have no child nodes that are interpreted in
// the enclosing context, only one context is needed for the defining
// node itself.
void MakeContext::visit(const TargetVar *op) {
    context_mgr.push(op);
    context_mgr.target(op->name, context_mgr.current_context());
}

void MakeContext::visit(const StmtTargetVar *op) {
    context_mgr.push(op);
    context_mgr.target(op->name, context_mgr.current_context());
}



} // end namespace IRLazyScopeInternal



namespace {
    // A simple tree walker runs over the tree.
    class Walker : public IRLazyScopeProcess {
    public:
        using IRLazyScopeProcess::visit;
        using IRLazyScopeProcess::process;
        
        // Count the number of references to variables of each type.
        int count_let, count_for, count_letstmt, count_notfound;
        
        Walker() : count_let(0), count_for(0), count_letstmt(0), count_notfound(0) {}
        
        //virtual void process(const Expr &expr) { std::cout << "Walker process " << expr << "\n"; IRLazyScopeBase::process(expr); }
        
        // Walk the tree.
        virtual void visit(const Variable *op) {
            //std::cout << "Walker visit Variable\n";
            // Look up to see whether the variable is defined.
            int found = find_variable(op->name);
            if (found) {
                //std::cout << ">>> " << current_context() << " Found " << op->name << "[" << found << "]" << "\n";
                const DefiningNode *def = call(found);
                const For *fornode = def->node().as<For>();
                const Let *let = def->node().as<Let>();
                const LetStmt *letstmt = def->node().as<LetStmt>();
                if (fornode) {
                    //std::cout << op->name << " bound to:\n" << Stmt(fornode);
                    count_for++;
                } else if (let) {
                    //std::cout << op->name << " bound to:\n" << Expr(let);
                    count_let++;
                } else if (letstmt) {
                    //std::cout << op->name << " bound to:\n" << Stmt(letstmt);
                    count_letstmt++;
                }
                ret(found);
                //std::cout << "[" << current_context() << "] Current context\n";
            } else {
                //std::cout << "Could not find " << op->name << " in context " << current_context() << "\n";
                count_notfound++;
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
    Expr select = new Select(x > 3, new Select(x < 87, input, new Cast(Int(16), y-17)),
                             new Cast(Int(16), -17));
    Stmt store = new Store("buf", select, x - 1);
    PartitionInfo partition(true);
    Stmt for_loop = new For("x", 0, 100, For::Parallel, partition, store);
    Stmt letstmt = new LetStmt("y", a * 2 + 5, for_loop); //a is undefined here
    Expr call = new Call(i32, "buf", vec(max(min(x,100),0)));
    Expr call2 = new Call(i32, "buf", vec(max(min(x-1,100),0)));
    Expr call3 = new Call(i32, "buf", vec(Expr(new Clamp(Clamp::Reflect, x+1, 0, 100))));
    Stmt store2 = new Store("out", call + call2 + call3 + 1 + y, x); // y is undefined here
    PartitionInfo partition2(InfInterval(1,99));
    Stmt for_loop2 = new For("x", 0, 100, For::Serial, partition2, store2);
    Stmt pipeline = new Pipeline("buf", letstmt, Stmt(), for_loop2);
    
    Walker walk;
    
    //std::cout << "Commence lazy scope test\n";
    walk.process(pipeline);
    const int def_for = 8;
    const int def_let = 0;
    const int def_letstmt = 1;
    const int def_notfound = 2;
    if (walk.count_for != def_for || walk.count_let != def_let || walk.count_letstmt != def_letstmt || 
        walk.count_notfound != def_notfound) {
        std::cout << "Lazy scope test failed.\n";
        std::cout << "For index variable reference count: " << walk.count_for << "   expected " << def_for << "\n";
        std::cout << "Let variable reference count: " << walk.count_let << "   expected " << def_let << "\n";
        std::cout << "LetStmt variable reference count: " << walk.count_letstmt << "   expected " << def_letstmt << "\n";
        std::cout << "Undefined variable reference count: " << walk.count_notfound << "   expected " << def_notfound << "\n";
        assert(0 && "Lazy scope test failed\n");
    }
    std::cout << "Lazy scope test completed\n";
    
    //std::cout << "Size of LazyScope is " << sizeof(IRLazyScope<IRProcess>) << "\n";
}

}
}


