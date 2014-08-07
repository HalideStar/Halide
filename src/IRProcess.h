#ifndef HALIDE_IR_PROCESS_H
#define HALIDE_IR_PROCESS_H

#include "IRVisitor.h"

/** \file
 * Defines a base class for recursively waling over the tree with
 * process method for high-level logic and visit methods for detail.
 * The process methods are called first, and then the visit methods.
 * This means that a base class can use the process method to
 * intercept tree nodes before a derived class accesses them with
 * visit.
 */

namespace Halide {
namespace Internal {

/** A base class for algorithms that need to recursively walk over the
 * IR. The default implementations just recursively walk over the
 * children. Override the ones you care about.
 * Walking is done by calling process instead of explicitly calling
 * node.accept(this).  By calling process, an override can happen before
 * the nodes are visited.
 */
class IRProcess : public IRVisitor {
public:
    virtual void process(const Stmt &stmt);
    virtual void process(const Expr &expr);
    
    virtual void visit(const IntImm *);
    virtual void visit(const FloatImm *);
    virtual void visit(const Cast *);
    virtual void visit(const Variable *);
    virtual void visit(const BitAnd *);
    virtual void visit(const BitOr *);
    virtual void visit(const BitXor *);
    virtual void visit(const SignFill *);
    virtual void visit(const Clamp *);
    virtual void visit(const Add *);
    virtual void visit(const Sub *);
    virtual void visit(const Mul *);
    virtual void visit(const Div *);
    virtual void visit(const Mod *);
    virtual void visit(const Min *);
    virtual void visit(const Max *);
    virtual void visit(const EQ *);
    virtual void visit(const NE *);
    virtual void visit(const LT *);
    virtual void visit(const LE *);
    virtual void visit(const GT *);
    virtual void visit(const GE *);
    virtual void visit(const And *);
    virtual void visit(const Or *);
    virtual void visit(const Not *);
    virtual void visit(const Select *);
    virtual void visit(const Load *);
    virtual void visit(const Ramp *);
    virtual void visit(const Broadcast *);
    virtual void visit(const Call *);
    virtual void visit(const Let *);
    virtual void visit(const LetStmt *);
    virtual void visit(const PrintStmt *);
    virtual void visit(const AssertStmt *);
    virtual void visit(const Pipeline *);
    virtual void visit(const For *);
    virtual void visit(const Store *);
    virtual void visit(const Provide *);
    virtual void visit(const Allocate *);
    virtual void visit(const Realize *);
    virtual void visit(const Block *);
    
    virtual void visit(const Solve *);
    virtual void visit(const TargetVar *);
    virtual void visit(const StmtTargetVar *);
    virtual void visit(const Infinity *);
};

}
}

#endif
