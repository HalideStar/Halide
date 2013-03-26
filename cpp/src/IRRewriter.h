#ifndef HALIDE_IR_REWRITER_H
#define HALIDE_IR_REWRITER_H

#include "IRVisitor.h"

/** \file
 * Defines the base class for rewriters to get applied by a tree walker.
 */

namespace Halide {
namespace Internal {

/** A base class for adding rewriting to an existing algorithm that walks over
 * the tree.  Override the methods for the nodes that you care about.
 * Pass the derived class object to a tree walker based on IRMutator.
 * That tree walker will provide some base functionality that you need,
 * and it will call your rewriter on any nodes that you are interested in.
 */
class IRRewriter {
public:
    // Sometimes, you need to know if IRRewriter has adopted
    // the default behaviour of doing nothing for a node for which
    // no explicit visit method is defined.
    // Boolean defaulted is set to true so the caller can detect that
    // this has happened and take some action.
    bool defaulted;
    IRRewriter();
    virtual ~IRRewriter();
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
    virtual void visit(const HDiv *);
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
    virtual void visit(const Letvoid );
    virtual void visit(const Printvoid );
    virtual void visit(const Assertvoid );
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
