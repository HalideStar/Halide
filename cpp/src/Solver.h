#ifndef HALIDE_SOLVER_H
#define HALIDE_SOLVER_H

/** \file
 * Methods for solving expressions of the form a <= e <= b.
 * This is also known as backwards interval analysis.
 */

#include "IR.h"
#include "DomInterval.h"

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

class Solution {
public:
    // var: The variable that we have solved for.
    // Note that Solve nodes in which the embedded solution is not a single variable
    // cannot be extracted as Solution.
    std::string var;
    
    // expr_source, stmt_source: The source node that was recorded in the TargetVar or StmtTargetVar
    // the identifies the variable in var, above.  One of these will be undefined.
    Expr expr_source;
    Stmt stmt_source;
    
    // Intervals that define the individual solutions.
    std::vector<DomInterval> intervals;
    
    Solution() {}
    Solution(std::string _var, Expr expr_s, Stmt stmt_s, std::vector<DomInterval> _intervals) : 
        var(_var), expr_source(expr_s), stmt_source(stmt_s), intervals(_intervals) {}
};
    
std::vector<Solution> extract_solutions(std::string var, Stmt source, Stmt solved);

std::vector<Solution> extract_solutions(std::string var, Expr source, Expr solved);
}
}

#endif
