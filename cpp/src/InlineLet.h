#ifndef HALIDE_INLINE_LET_H
#define HALIDE_INLINE_LET_H

//LH

/** \file
 *
 * Aggressive inlining of Let expressions/statements.
 */

#include "IR.h"
#include "IRMutator.h"
#include "Scope.h"
#include <map>

namespace Halide {
namespace Internal {

EXPORT std::vector<std::string> list_repeat_variables(Expr e);


/** Class/base class to aggressively inline let expressions.
 * This class inlines Let expresions and LetStmt as much as possible.
 * The only restriction is that the value of the Let may not contain
 * a reference to a variable that has been defined more recently
 * than the Let itself. */
class InlineLet : public IRMutator {
    Scope<Expr> scope;
    
public:
    using IRMutator::visit;

    InlineLet() {}
    
protected:
    // Visit a variable to possibly inline it.
    virtual void visit(const Variable *op);
    // Visit a Let expression, LetStmt to bind the definition
    virtual void visit(const Let *op); 
    virtual void visit(const LetStmt *op);
    // Visit a For node to note that the variable is defined there.
    virtual void visit(const For *op);
};

}
}

#endif
