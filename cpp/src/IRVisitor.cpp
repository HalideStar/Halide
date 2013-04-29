#include "IRVisitor.h"
#include "IR.h"
#include "IRPrinter.h"
#include <ostream>

namespace Halide { 
namespace Internal {

IRVisitor::IRVisitor() {
    defaulted = false;
}

IRVisitor::~IRVisitor() {
}

void IRVisitor::process(const Stmt &stmt) {
    std::cout << "IRVisitor process " << stmt << "\n";
    stmt.accept(this);
}

void IRVisitor::process(const Expr& expr) {
    std::cout << "IRVisitor process " << expr << "\n";
    expr.accept(this);
}

void IRVisitor::visit(const IntImm *) {
    defaulted = true;
}
    
void IRVisitor::visit(const FloatImm *) {
    defaulted = true;
}
    
void IRVisitor::visit(const Cast *op) {
    defaulted = true;
    op->value.accept(this);
}
    
void IRVisitor::visit(const Variable *) {
    defaulted = true;
}

//LH
void IRVisitor::visit(const BitAnd *op) {
    defaulted = true;
    op->a.accept(this);
    op->b.accept(this);
}

//LH
void IRVisitor::visit(const BitOr *op) {
    defaulted = true;
    op->a.accept(this);
    op->b.accept(this);
}

//LH
void IRVisitor::visit(const BitXor *op) {
    defaulted = true;
    op->a.accept(this);
    op->b.accept(this);
}

//LH
void IRVisitor::visit(const SignFill *op) {
    defaulted = true;
    op->value.accept(this);
}

//LH
void IRVisitor::visit(const Clamp *op) {
    defaulted = true;
    op->a.accept(this);
	op->min.accept(this);
	op->max.accept(this);
	if (op->clamptype == Internal::Clamp::Tile)
		op->p1.accept(this);
}

void IRVisitor::visit(const Add *op) {
    defaulted = true;
    op->a.accept(this);
    op->b.accept(this);
}

void IRVisitor::visit(const Sub *op) {
    defaulted = true;
    op->a.accept(this);
    op->b.accept(this);
}

void IRVisitor::visit(const Mul *op) {
    defaulted = true;
    op->a.accept(this);
    op->b.accept(this);
}

void IRVisitor::visit(const Div *op) {
    defaulted = true;
    op->a.accept(this);
    op->b.accept(this);
}

void IRVisitor::visit(const Mod *op) {
    defaulted = true;
    op->a.accept(this);
    op->b.accept(this);
}

void IRVisitor::visit(const Min *op) {
    defaulted = true;
    op->a.accept(this);
    op->b.accept(this);
}

void IRVisitor::visit(const Max *op) {
    defaulted = true;
    op->a.accept(this);
    op->b.accept(this);
}

void IRVisitor::visit(const EQ *op) {
    defaulted = true;
    op->a.accept(this);
    op->b.accept(this);
}

void IRVisitor::visit(const NE *op) {
    defaulted = true;
    op->a.accept(this);
    op->b.accept(this);
}

void IRVisitor::visit(const LT *op) {
    defaulted = true;
    op->a.accept(this);
    op->b.accept(this);
}

void IRVisitor::visit(const LE *op) {
    defaulted = true;
    op->a.accept(this);
    op->b.accept(this);
}

void IRVisitor::visit(const GT *op) {
    defaulted = true;
    op->a.accept(this);
    op->b.accept(this);
}

void IRVisitor::visit(const GE *op) {
    defaulted = true;
    op->a.accept(this);
    op->b.accept(this);
}

void IRVisitor::visit(const And *op) {
    defaulted = true;
    op->a.accept(this);
    op->b.accept(this);
}        

void IRVisitor::visit(const Or *op) {
    defaulted = true;
    op->a.accept(this);
    op->b.accept(this);
}

void IRVisitor::visit(const Not *op) {
    defaulted = true;
    op->a.accept(this);
}
    
void IRVisitor::visit(const Select *op) {
    defaulted = true;
    op->condition.accept(this);
    op->true_value.accept(this);
    op->false_value.accept(this);
}

void IRVisitor::visit(const Load *op) {
    defaulted = true;
    op->index.accept(this);
}

void IRVisitor::visit(const Ramp *op) {
    defaulted = true;
    op->base.accept(this);
    op->stride.accept(this);
}

void IRVisitor::visit(const Broadcast *op) {
    defaulted = true;
    op->value.accept(this);
}

void IRVisitor::visit(const Call *op) {
    defaulted = true;
    for (size_t i = 0; i < op->args.size(); i++) {
        op->args[i].accept(this);
    }
}

void IRVisitor::visit(const Let *op) {
    defaulted = true;
    op->value.accept(this);
    op->body.accept(this);
}

void IRVisitor::visit(const LetStmt *op) {
    defaulted = true;
    op->value.accept(this);
    op->body.accept(this);
}

void IRVisitor::visit(const PrintStmt *op) {
    defaulted = true;
    for (size_t i = 0; i < op->args.size(); i++) {
        op->args[i].accept(this);
    }
}

void IRVisitor::visit(const AssertStmt *op) {
    defaulted = true;
    op->condition.accept(this);
}

void IRVisitor::visit(const Pipeline *op) {
    defaulted = true;
    op->produce.accept(this);
    if (op->update.defined()) op->update.accept(this);
    op->consume.accept(this);
}

void IRVisitor::visit(const For *op) {
    defaulted = true;
    op->min.accept(this);
    op->extent.accept(this);
    op->body.accept(this);
}

void IRVisitor::visit(const Store *op) {
    defaulted = true;
    op->value.accept(this);
    op->index.accept(this);
}

void IRVisitor::visit(const Provide *op) {
    defaulted = true;
    op->value.accept(this);
    for (size_t i = 0; i < op->args.size(); i++) {
        op->args[i].accept(this);
    }
}

void IRVisitor::visit(const Allocate *op) {
    defaulted = true;
    op->size.accept(this);
    op->body.accept(this);
}

void IRVisitor::visit(const Realize *op) {
    defaulted = true;
    for (size_t i = 0; i < op->bounds.size(); i++) {
        op->bounds[i].min.accept(this);
        op->bounds[i].extent.accept(this);
    }
    op->body.accept(this);
}

void IRVisitor::visit(const Block *op) {
    defaulted = true;
    op->first.accept(this);
    if (op->rest.defined()) op->rest.accept(this);
}

void IRVisitor::visit(const Solve *op) {
    defaulted = true;
    op->e.accept(this);
    // Does not visit the Solve data, the intervals.
}
    
void IRVisitor::visit(const TargetVar *op) {
    defaulted = true;
    op->body.accept(this);
}
    
void IRVisitor::visit(const StmtTargetVar *op) {
    defaulted = true;
    op->body.accept(this);
}
    
void IRVisitor::visit(const Infinity *op) {
    defaulted = true;
}
    

}
}


