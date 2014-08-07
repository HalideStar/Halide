#ifndef HALIDE_SIMPLIFY_BOUNDS_H
#define HALIDE_SIMPLIFY_BOUNDS_H

#include "IR.h"

/** \file 
 * Methods for simplifying an expression or statement using bounds analysis
 * (i.e. bounds of expressions).
 */

namespace Halide {
namespace Internal {


Stmt bounds_simplify(Stmt s);
Expr bounds_simplify(Expr e);

void bounds_simplify_test();
        
}
}

#endif
