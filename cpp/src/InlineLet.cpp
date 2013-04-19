#include "InlineLet.h"

namespace Halide {
namespace Internal {

using std::string;
using std::map;

namespace {
// ListVariables walks an argument expression and returns a list of
// all the variables found in it.  The list may contain repeat elements.

class ListRepeatVariables : public IRVisitor {
public:
    std::vector<std::string> varlist;
    ListRepeatVariables() {}
    
private:
    using IRVisitor::visit;

    void visit(const Variable *op) {
        varlist.push_back(op->name);
    }
};
}

// list_repeat_variables returns a list of the variables found in an expression.
// Variables that are referenced more than once will be listed more than once.

std::vector<std::string> list_repeat_variables(Expr e) {
    ListRepeatVariables lister;
    e.accept(&lister);
    return lister.varlist;
}

void InlineLet::visit(const Variable *op) {
    if (scope.contains(op->name)) {
        Expr replacement = scope.get(op->name);

        //std::cout << "Pondering replacing " << op->name << " with " << replacement << std::endl;

        // if expr is defined, we should substitute it in (unless
        // it's a var that has been hidden by a nested scope).
        if (replacement.defined()) {
            //std::cout << "Replacing " << op->name << " of type " << op->type << " with " << replacement << std::endl;
            assert(replacement.type() == op->type);
            // If it's a naked var, and the var it refers to
            // hasn't gone out of scope, just replace it with that
            // var
            if (const Variable *v = replacement.as<Variable>()) {
                if (scope.contains(v->name)) {
                    if (scope.depth(v->name) < scope.depth(op->name)) {
                        expr = replacement;
                    } else {
                        // Uh oh, the variable we were going to
                        // subs in has been hidden by another
                        // variable of the same name, better not
                        // do anything.
                        expr = op;
                    }
                } else {
                    // It is a variable, but the variable this
                    // refers to hasn't been encountered. It must
                    // be a uniform, so it's safe to substitute it
                    // in.
                    expr = replacement;
                }
            } else {
                // It's not a variable, and a replacement is defined
                expr = replacement;
            }
        } else {
            // This expression was not something deemed
            // substitutable - no replacement is defined.
            expr = op;
        }
    } else {
        // We never encountered a let that defines this var. Must
        // be a uniform. Don't touch it.
        expr = op;
    }
}
    
template<typename T, typename Body> 
Body simplify_let(const T *op, Scope<Expr> &scope, IRMutator *mutator) {
    // Aggressively inline Let.
    Expr value = mutator->mutate(op->value);
    Body body = op->body;
    assert(value.defined());
    assert(body.defined());
    // Substitute the value wherever we see it.
    // If the value is a variable, it will already have been expanded except
    // for a global variable or a For loop index variable.
    scope.push(op->name, value);
    
    body = mutator->mutate(body);

    scope.pop(op->name);

    if (body.same_as(op->body) && value.same_as(op->value)) {
        return op;
    } else {
        return new T(op->name, value, body);
    }        
}


void InlineLet::visit(const Let *op) {
    expr = simplify_let<Let, Expr>(op, scope, this);
}

void InlineLet::visit(const LetStmt *op) {
    stmt = simplify_let<LetStmt, Stmt>(op, scope, this);
}

void InlineLet::visit(const For *op) {
    Expr min = mutate(op->min);
    Expr extent = mutate(op->extent);
    
    scope.push(op->name, Expr()); // For loop overrides variable.
    Stmt body = mutate(op->body);
    scope.pop(op->name);
    
    if (body.same_as(op->body) && min.same_as(op->min) && extent.same_as(op->extent)) {
        stmt = op;
    } else {
        stmt = new For(op, min, extent, body);
    }        
}

# if 0
void loop_partition_test() {
    test_loop_partition_1();
    
    std::cout << "Loop Partition test passed\n";
}
# endif

}
}
