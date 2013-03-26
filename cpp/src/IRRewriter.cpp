#include "IRRewriter.h"
#include "IR.h"

namespace Halide { 
namespace Internal {

IRRewriter::IRRewriter() {
    defaulted = false;
}

IRRewriter::~IRRewriter() {
}
    
void IRRewriter::visit(const IntImm *op) {
    defaulted = true;
    expr = op;
}
    
void IRRewriter::visit(const FloatImm *op) {
    defaulted = true;
    expr = op;
}
    
void IRRewriter::visit(const Cast *op) {
    defaulted = true;
    expr = op;
}
    
void IRRewriter::visit(const Variable *) {
    defaulted = true;
    expr = op;
}

//LH
void IRRewriter::visit(const BitAnd *op) {
    defaulted = true;
    expr = op;
}

//LH
void IRRewriter::visit(const BitOr *op) {
    defaulted = true;
    expr = op;
}

//LH
void IRRewriter::visit(const BitXor *op) {
    defaulted = true;
    expr = op;
}

//LH
void IRRewriter::visit(const SignFill *op) {
    defaulted = true;
    expr = op;
}

//LH
void IRRewriter::visit(const Clamp *op) {
    defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Add *op) {
    defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Sub *op) {
    defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Mul *op) {
    defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Div *op) {
    defaulted = true;
    expr = op;
}

//LH
void IRRewriter::visit(const HDiv *op) {
    defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Mod *op) {
    defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Min *op) {
    defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Max *op) {
    defaulted = true;
    expr = op;
}

void IRRewriter::visit(const EQ *op) {
    defaulted = true;
    expr = op;
}

void IRRewriter::visit(const NE *op) {
    defaulted = true;
    expr = op;
}

void IRRewriter::visit(const LT *op) {
    defaulted = true;
    expr = op;
}

void IRRewriter::visit(const LE *op) {
    defaulted = true;
    expr = op;
}

void IRRewriter::visit(const GT *op) {
    defaulted = true;
    expr = op;
}

void IRRewriter::visit(const GE *op) {
    defaulted = true;
    expr = op;
}

void IRRewriter::visit(const And *op) {
    defaulted = true;
    expr = op;
}        

void IRRewriter::visit(const Or *op) {
    defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Not *op) {
    defaulted = true;
    expr = op;
}
    
void IRRewriter::visit(const Select *op) {
    defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Load *op) {
    defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Ramp *op) {
    defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Broadcast *op) {
    defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Call *op) {
    defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Let *op) {
    defaulted = true;
    expr = op;
}

void IRRewriter::visit(const Letvoid op) {
    defaulted = true;
    stmt = op;
}

void IRRewriter::visit(const Printvoid op) {
    defaulted = true;
    stmt = op;
}

void IRRewriter::visit(const Assertvoid op) {
    defaulted = true;
    stmt = op;
}

void IRRewriter::visit(const Pipeline *op) {
    defaulted = true;
    stmt = op;
}

void IRRewriter::visit(const For *op) {
    defaulted = true;
    stmt = op;
}

void IRRewriter::visit(const Store *op) {
    defaulted = true;
    stmt = op;
}

void IRRewriter::visit(const Provide *op) {
    defaulted = true;
    stmt = op;
}

void IRRewriter::visit(const Allocate *op) {
    defaulted = true;
    stmt = op;
}

void IRRewriter::visit(const Realize *op) {
    defaulted = true;
    stmt = op;
}

void IRRewriter::visit(const Block *op) {
    defaulted = true;
    stmt = op;
}

}
}


