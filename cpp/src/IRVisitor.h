#ifndef HALIDE_IR_VISITOR_H
#define HALIDE_IR_VISITOR_H

/** \file
 * Defines the base class for things that recursively walk over the IR
 */

namespace Halide {

struct Expr;

namespace Internal {

struct Stmt;
struct IntImm;
struct FloatImm;
struct Cast;
struct Variable;
struct BitAnd; //LH
struct BitOr; //LH
struct BitXor; //LH
struct SignFill; //LH
struct Clamp; //LH
struct Add;
struct Sub;
struct Mul;
struct Div;
struct Mod;
struct Min;
struct Max;
struct EQ;
struct NE;
struct LT;
struct LE;
struct GT;
struct GE;
struct And;
struct Or;
struct Not;
struct Select;
struct Load;
struct Ramp;
struct Broadcast;
struct Call;
struct Let;
struct LetStmt;
struct PrintStmt;
struct AssertStmt;
struct Pipeline;
struct For;
struct Store;
struct Provide;
struct Allocate;
struct Realize;
struct Block;

/** A base class for algorithms that need to recursively walk over the
 * IR. The default implementations just recursively walk over the
 * children. Override the ones you care about.
 */
class IRVisitor {
public:
    // Sometimes, you need to know if IRVisitor has adopted
    // the default behaviour of visiting all the children
    // of a node for which no explicit visit method is defined.
    // Boolean defaulted is set to true so the caller can detect that
    // this has happened and take some action.
    // Of course, if the derived class uses the base class to visit the
    // children then that will also set defaulted to true.
    bool defaulted;
    IRVisitor();
    virtual ~IRVisitor();
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
};

}
}

#endif
