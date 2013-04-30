#include "IRProcess.h"
#include "IR.h"
#include "IRPrinter.h"
#include <ostream>

namespace Halide { 
namespace Internal {

void IRProcess::process(const Stmt &stmt) { stmt.accept(this); }
void IRProcess::process(const Expr &expr) { expr.accept(this); }

void IRProcess::visit(const IntImm *) {
}
    
void IRProcess::visit(const FloatImm *) {
}
    
void IRProcess::visit(const Cast *op) {
    process(op->value);
}
    
void IRProcess::visit(const Variable *op) {
    std::cout << "IRProcess visit " << Expr(op) << "\n";
}

//LH
void IRProcess::visit(const BitAnd *op) {
    process(op->a);
    process(op->b);
}

//LH
void IRProcess::visit(const BitOr *op) {
    process(op->a);
    process(op->b);
}

//LH
void IRProcess::visit(const BitXor *op) {
    process(op->a);
    process(op->b);
}

//LH
void IRProcess::visit(const SignFill *op) {
    process(op->value);
}

//LH
void IRProcess::visit(const Clamp *op) {
    process(op->a);
	process(op->min);
	process(op->max);
	if (op->clamptype == Internal::Clamp::Tile)
		process(op->p1);
}

void IRProcess::visit(const Add *op) {
    process(op->a);
    process(op->b);
}

void IRProcess::visit(const Sub *op) {
    process(op->a);
    process(op->b);
}

void IRProcess::visit(const Mul *op) {
    process(op->a);
    process(op->b);
}

void IRProcess::visit(const Div *op) {
    process(op->a);
    process(op->b);
}

void IRProcess::visit(const Mod *op) {
    process(op->a);
    process(op->b);
}

void IRProcess::visit(const Min *op) {
    process(op->a);
    process(op->b);
}

void IRProcess::visit(const Max *op) {
    process(op->a);
    process(op->b);
}

void IRProcess::visit(const EQ *op) {
    process(op->a);
    process(op->b);
}

void IRProcess::visit(const NE *op) {
    process(op->a);
    process(op->b);
}

void IRProcess::visit(const LT *op) {
    process(op->a);
    process(op->b);
}

void IRProcess::visit(const LE *op) {
    process(op->a);
    process(op->b);
}

void IRProcess::visit(const GT *op) {
    process(op->a);
    process(op->b);
}

void IRProcess::visit(const GE *op) {
    process(op->a);
    process(op->b);
}

void IRProcess::visit(const And *op) {
    process(op->a);
    process(op->b);
}        

void IRProcess::visit(const Or *op) {
    process(op->a);
    process(op->b);
}

void IRProcess::visit(const Not *op) {
    process(op->a);
}
    
void IRProcess::visit(const Select *op) {
    process(op->condition);
    process(op->true_value);
    process(op->false_value);
}

void IRProcess::visit(const Load *op) {
    process(op->index);
}

void IRProcess::visit(const Ramp *op) {
    process(op->base);
    process(op->stride);
}

void IRProcess::visit(const Broadcast *op) {
    process(op->value);
}

void IRProcess::visit(const Call *op) {
    for (size_t i = 0; i < op->args.size(); i++) {
        process(op->args[i]);
    }
}

void IRProcess::visit(const Let *op) {
    process(op->value);
    process(op->body);
}

void IRProcess::visit(const LetStmt *op) {
    process(op->value);
    process(op->body);
}

void IRProcess::visit(const PrintStmt *op) {
    for (size_t i = 0; i < op->args.size(); i++) {
        process(op->args[i]);
    }
}

void IRProcess::visit(const AssertStmt *op) {
    process(op->condition);
}

void IRProcess::visit(const Pipeline *op) {
    process(op->produce);
    if (op->update.defined()) process(op->update);
    process(op->consume);
}

void IRProcess::visit(const For *op) {
    process(op->min);
    process(op->extent);
    process(op->body);
}

void IRProcess::visit(const Store *op) {
    process(op->value);
    process(op->index);
}

void IRProcess::visit(const Provide *op) {
    process(op->value);
    for (size_t i = 0; i < op->args.size(); i++) {
        process(op->args[i]);
    }
}

void IRProcess::visit(const Allocate *op) {
    process(op->size);
    process(op->body);
}

void IRProcess::visit(const Realize *op) {
    for (size_t i = 0; i < op->bounds.size(); i++) {
        process(op->bounds[i].min);
        process(op->bounds[i].extent);
    }
    process(op->body);
}

void IRProcess::visit(const Block *op) {
    process(op->first);
    if (op->rest.defined()) process(op->rest);
}

void IRProcess::visit(const Solve *op) {
    process(op->body);
    // Does not visit the Solve data, i.e. the intervals.
}
    
void IRProcess::visit(const TargetVar *op) {
    process(op->body);
    // Does not visit the source, which is a link to the source tree.
}
    
void IRProcess::visit(const StmtTargetVar *op) {
    process(op->body);
    // Does not visit the source, which is a link to the source tree.
}
    
void IRProcess::visit(const Infinity *op) {
}
    

}
}


