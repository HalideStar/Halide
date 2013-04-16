#ifndef HALIDE_SOLVER_H
#define HALIDE_SOLVER_H

/** \file
 * Methods for solving expressions of the form a <= e <= b.
 * This is also known as backwards interval analysis.
 */

#include "IR.h"

namespace Halide { 
namespace Internal {

// @{
//Stmt simplify(Stmt);
//Expr simplify(Expr);
// @}  

void solver_test();

}
}

#endif
