#ifndef HALIDE_INTERVAL_ANALYSIS_H
#define HALIDE_INTERVAL_ANALYSIS_H

#include "IR.h"
#include "IRVisitor.h"
#include "Scope.h"
#include <utility>
#include <vector>

/** \file 
 * Methods for computing the upper and lower bounds of an expression,
 * and the regions of a function read or written by a statement. 
 */

namespace Halide {
namespace Internal {

/** Given an expression in some variables, and a map from those
 * variables to their bounds (in the form of (minimum possible value,
 * maximum possible value)), compute two expressions that give the
 * minimum possible value and the maximum possible value of this
 * expression. Max or min may be undefined expressions if the value is
 * not bounded above or below.
 *
 * This is for tasks such as deducing the region of a buffer
 * loaded by a chunk of code.
 */
Interval interval_of_expr_in_scope(Expr expr, const Scope<Interval> &scope);    

/** Call bounds_of_expr_in_scope with an empty scope */
//Interval bounds_of_expr(Expr expr);

void interval_analysis_test();
        
}
}

#endif
