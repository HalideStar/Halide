#ifndef HALIDE_IR_PRINTER_H
#define HALIDE_IR_PRINTER_H

#include "IRVisitor.h"
#include "Type.h"
#include "IR.h"
#include "Interval.h"
#include "DomainInference.h"
#include <ostream>

/** \file 
 * This header file defines operators that let you dump a Halide
 * expression, statement, or type directly into an output stream
 * in a human readable form.
 * E.g:
 \code
 Expr foo = ...
 std::cout << "Foo is " << foo << std::endl;
 \endcode
 * 
 * These operators are implemented using \ref Halide::Internal::IRPrinter
 */

/** Emit a vector of Interval in human readable form.  Must be defined in namespace std because
 * C++ looks for a definition associated with std::vector in namespace std. */ 
namespace std {
std::ostream &operator<<(std::ostream &stream, const std::vector<Halide::Expr> &v);
std::ostream &operator<<(std::ostream &stream, const std::vector<Halide::Interval> &v);
}

namespace Halide { 

/** Emit an expression on an output stream (such as std::cout) in a
 * human-readable form */
std::ostream &operator<<(std::ostream &stream, Expr);

class Func;

/** Emit a function on an output stream (such as std::cout) in a
 * human-readable form */
std::ostream &operator<<(std::ostream &stream, Func);

/** Emit a halide type on an output stream (such as std::cout) in a
 * human-readable form */
std::ostream &operator<<(std::ostream &stream, Type);

/** Emit a halide Domain in a human readable form */
std::ostream &operator<<(std::ostream &stream, Domain);

/** Emit an Interval in human readable form */
std::ostream &operator<<(std::ostream &stream, Halide::Interval v);

namespace Internal {

/** Emit a halide statement on an output stream (such as std::cout) in
 * a human-readable form */
std::ostream &operator<<(std::ostream &stream, Stmt);

/** An IRVisitor that emits IR to the given output stream in a human
 * readable form. Can be subclassed if you want to modify the way in
 * which it prints.
 */
class IRPrinter : public IRVisitor {
    // The absence of using statement in this class means that all visit
    // methods must be defined.
public:
    /** Construct an IRPrinter pointed at a given output stream
     * (e.g. std::cout, or a std::ofstream) */
    IRPrinter(std::ostream &);

    /** emit an expression on the output stream */
    void print(Expr);

    /** emit a statement on the output stream */
    void print(Stmt);

    static void test();

protected:
    /** The stream we're outputting on */
    std::ostream &stream;

    /** The current indentation level, useful for pretty-printing
     * statements */
    int indent;

    /** Emit spaces according to the current indentation level */
    void do_indent();

    void visit(const IntImm *);
    void visit(const FloatImm *);
    void visit(const Cast *);
    void visit(const Variable *);
    void visit(const BitAnd *); //LH
    void visit(const BitOr *); //LH
    void visit(const BitXor *); //LH
    void visit(const SignFill *); //LH
    void visit(const Clamp *); //LH
    void visit(const Add *);
    void visit(const Sub *);
    void visit(const Mul *);
    void visit(const Div *);
    void visit(const Mod *);
    void visit(const Min *);
    void visit(const Max *);
    void visit(const EQ *);
    void visit(const NE *);
    void visit(const LT *);
    void visit(const LE *);
    void visit(const GT *);
    void visit(const GE *);
    void visit(const And *);
    void visit(const Or *);
    void visit(const Not *);
    void visit(const Select *);
    void visit(const Load *);
    void visit(const Ramp *);
    void visit(const Broadcast *);
    void visit(const Call *);
    void visit(const Let *);
    void visit(const LetStmt *);
    void visit(const PrintStmt *);
    void visit(const AssertStmt *);
    void visit(const Pipeline *);
    void visit(const For *);
    void visit(const Store *);
    void visit(const Provide *);
    void visit(const Allocate *);
    void visit(const Realize *);
    void visit(const Block *);

    void visit(const Solve *);
    void visit(const TargetVar *);
    void visit(const StmtTargetVar *);
    void visit(const Infinity *);
};
}
}

#endif
