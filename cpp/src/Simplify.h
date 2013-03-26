#ifndef HALIDE_SIMPLIFY_H
#define HALIDE_SIMPLIFY_H

/** \file
 * Methods for simplifying halide statements and expressions 
 */

#include "IR.h"

namespace Halide { 
namespace Internal {

/** Perform a a wide range of simplifications to expressions
 * and statements, including constant folding, substituting in
 * trivial values, arithmetic rearranging, etc.
 */
// @{
Stmt simplify(Stmt);
Expr simplify(Expr);
// @}  

/** Perform simplification, but return undefined expression instead of
 * error for undefined expression as input. */
Stmt simplify_undef(Stmt s) { return s.defined() ? simplify(s) : s; }
Expr simplify_undef(Expr e) { return e.defined() ? simplify(e) : e; }

/** Use the simplifier to test whether an expression can be
 * evaluated to true.
 */
// @{
bool proved(Expr);
// @}
   
void simplify_test();

}
}

#endif
