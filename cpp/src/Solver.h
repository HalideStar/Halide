#ifndef HALIDE_SOLVER_H
#define HALIDE_SOLVER_H

/** \file
 * Methods for solving expressions of the form a <= e <= b.
 * This is also known as backwards interval analysis.
 */

#include "IR.h"

namespace Halide { 
namespace Internal {

/** Run the solver over a tree.
 * It interprets Solve nodes as things to be solved,
 * and returns a tree in which those Solve nodes become solutions.
 * It also interprets TargetVar nodes as providing context - the named
 * variables become targets for solutions within the subtree.
 *
 * To use the solver, first mutate the tree to insert TargetVar and Solve
 * nodes as appropriate to your application.  Then run the solver.
 * Finally, extract the solutions from the tree and discard the tree.
 * If you want additional functionality in the solver, derive a class from it
 * and add your new functionality.
 */
EXPORT Stmt solver(Stmt s);
EXPORT Expr solver(Expr e);

EXPORT bool is_constant_expr(std::vector<std::string> varlist, Expr e);

EXPORT void solver_test();

}
}

#endif
