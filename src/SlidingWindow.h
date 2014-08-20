#ifndef HALIDE_SLIDING_WINDOW_H
#define HALIDE_SLIDING_WINDOW_H

/** \file
 *
 * Defines the sliding_window lowering optimization pass, which avoids
 * computing provably-already-computed values.
 */

#include "IR.h"
#include <map>

namespace Halide {
namespace Internal {

/** Perform sliding window optimizations on a halide
 * statement. I.e. don't bother computing points in a function that
 * have provably already been computed by a previous iteration.
 */
Stmt sliding_window(Stmt s, const std::map<std::string, Function> &env);

/** Utility routine to determine whether an expression/statement depends on a variable. */
bool expr_depends_on_var(Expr e, std::string var);
bool stmt_depends_on_var(Stmt e, std::string var);
 
}
}

#endif
