#ifndef HALIDE_SIMPLIFY_H
#define HALIDE_SIMPLIFY_H

/** \file
 * Methods for simplifying halide statements and expressions 
 */

#include "IR.h"
#include <cmath>

// Lift constants out of Min and Max expressions?  i.e. Min(e+k,k2) --> Min(e,k2-k)+k
// This definition is used in Simplify.cpp to modify the rules, and in LoopPartition.cpp
// to correctly detect the results of applying the rules.
# define LIFT_CONSTANT_MIN_MAX 0

namespace Halide { 
namespace Internal {

/** Perform a a wide range of simplifications to expressions and
 * statements, including constant folding, substituting in trivial
 * values, arithmetic rearranging, etc.
 */
// @{
Stmt simplify(Stmt);
Expr simplify(Expr);
// @}  

/** Use the simplifier to test whether an expression can be
 * evaluated to true.  Disproved parameter (if used) tests
 * whether the expression can be evaluated to false.
 */
// @{
bool proved(Expr, bool& disproved);
bool proved(Expr);
// @}

/** Use the simplifier to test whether either of two expressions
 * can be evaluated to true.  Disproved parameter tests whether
 * both evaluate to false.
 */
// @{
bool proved_either(Expr e1, Expr e2, bool &disproved);
// @}
   
/** Implementations of division and mod that are specific to Halide.
 * Use these implementations; do not use native C division or mod to simplify
 * Halide expressions. */
template<typename T>
inline T mod_imp(T a, T b) {
    T rem = a % b;
    Type t = type_of<T>();
    if (t.is_int()) {
        rem = rem + (rem != 0 && (rem ^ b) < 0 ? b : 0);
    }
    return rem;
}
// Special cases for float, double.
template<> inline float mod_imp<float>(float a, float b) { 
    float f = a - b * (floorf(a / b));
    // The remainder has the same sign as b.
    return f; 
}
template<> inline double mod_imp<double>(double a, double b) {
    double f = a - b * (std::floor(a / b));
    return f; 
}

// Division that rounds the quotient down for integers.
template<typename T>
inline T div_imp(T a, T b) {
    Type t = type_of<T>();
    T quotient;
    if (t.is_int()) {
        T axorb = a ^ b;
        T post = a != 0 ? ((axorb) >> (t.bits-1)) : 0;
        T pre = a < 0 ? -post : post;
        T num = a + pre;
        T q = num / b;
        quotient = q + post;
    } else {
        quotient = a / b;
    }
    return quotient; 
}

void simplify_clear();

void simplify_test();

}
}

#endif
