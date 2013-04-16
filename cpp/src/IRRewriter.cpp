#include "IRRewriter.h"
#include "IR.h"
#include "Log.h"
#include "IRPrinter.h"

namespace Halide { 
namespace Internal {

IRRewriter::IRRewriter() {
    rewriter_defaulted = false;
}

IRRewriter::~IRRewriter() {
}
    
void IRRewriter::visit(const IntImm *op) {
    rewriter_defaulted = true;
    expr = op;
}
    
void IRRewriter::visit(const FloatImm *op) {
    rewriter_defaulted = true;
    expr = op;
}
    
void IRRewriter::visit(const Cast *op) {
    rewriter_defaulted = true;
    expr = op;
}
    
void IRRewriter::visit(const Variable *op) {
    rewriter_defaulted = true;
    expr = op;
}

//LH
void IRRewriter::visit(const BitAnd *op) {
    rewriter_defaulted = true;
    expr = op;
}

//LH
void IRRewriter::visit(const BitOr *op) {
    rewriter_defaulted = true;
    expr = op;
}

//LH
void IRRewriter::visit(const BitXor *op) {
    rewriter_defaulted = true;
    expr = op;
}

//LH
void IRRewriter::visit(const SignFill *op) {
    rewriter_defaulted = true;
    expr = op;
}

//LH
void IRRewriter::visit(const Clamp *op) {
    rewriter_defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Add *op) {
    rewriter_defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Sub *op) {
    rewriter_defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Mul *op) {
    rewriter_defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Div *op) {
    rewriter_defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Mod *op) {
    rewriter_defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Min *op) {
    rewriter_defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Max *op) {
    rewriter_defaulted = true;
    expr = op;
}

void IRRewriter::visit(const EQ *op) {
    rewriter_defaulted = true;
    expr = op;
}

void IRRewriter::visit(const NE *op) {
    rewriter_defaulted = true;
    expr = op;
}

void IRRewriter::visit(const LT *op) {
    rewriter_defaulted = true;
    expr = op;
}

void IRRewriter::visit(const LE *op) {
    rewriter_defaulted = true;
    expr = op;
}

void IRRewriter::visit(const GT *op) {
    rewriter_defaulted = true;
    expr = op;
}

void IRRewriter::visit(const GE *op) {
    rewriter_defaulted = true;
    expr = op;
}

void IRRewriter::visit(const And *op) {
    rewriter_defaulted = true;
    expr = op;
}        

void IRRewriter::visit(const Or *op) {
    rewriter_defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Not *op) {
    rewriter_defaulted = true;
    expr = op;
}
    
void IRRewriter::visit(const Select *op) {
    rewriter_defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Load *op) {
    rewriter_defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Ramp *op) {
    rewriter_defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Broadcast *op) {
    rewriter_defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Call *op) {
    rewriter_defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Let *op) {
    rewriter_defaulted = true;
    expr = op;
}

void IRRewriter::visit(const LetStmt *op) {
    rewriter_defaulted = true;
    stmt = op;
}

void IRRewriter::visit(const PrintStmt *op) {
    rewriter_defaulted = true;
    stmt = op;
}

void IRRewriter::visit(const AssertStmt *op) {
    rewriter_defaulted = true;
    stmt = op;
}

void IRRewriter::visit(const Pipeline *op) {
    rewriter_defaulted = true;
    stmt = op;
}

void IRRewriter::visit(const For *op) {
    rewriter_defaulted = true;
    stmt = op;
}

void IRRewriter::visit(const Store *op) {
    rewriter_defaulted = true;
    stmt = op;
}

void IRRewriter::visit(const Provide *op) {
    rewriter_defaulted = true;
    stmt = op;
}

void IRRewriter::visit(const Allocate *op) {
    rewriter_defaulted = true;
    stmt = op;
}

void IRRewriter::visit(const Realize *op) {
    rewriter_defaulted = true;
    stmt = op;
}

void IRRewriter::visit(const Block *op) {
    rewriter_defaulted = true;
    stmt = op;
}

void IRRewriter::visit(const Solve *op) {
    rewriter_defaulted = true;
    expr = op;
}

void IRRewriter::visit(const TargetVar *op) {
    rewriter_defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Infinity *op) {
    rewriter_defaulted = true;
    expr = op;
}

}
}


