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
