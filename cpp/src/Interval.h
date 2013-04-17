#ifndef HALIDE_INTERVAL_H
#define HALIDE_INTERVAL_H

/** \file
 * Define an Interval: a range from min to max inclusive.
 */

namespace Halide {

namespace Internal {

/** An interval.  Includes all numbers from min to max inclusive. */
struct Interval {
    Expr min, max;
    Interval(Expr min, Expr max) : min(min), max(max) {}
    Interval() {}
    
};


/** C++ functions to manipulate an Interval object, returning a new Interval as the result */
EXPORT Interval operator+(Interval v, Expr b);
EXPORT Interval operator-(Interval v); // Unary negation
EXPORT Interval operator-(Interval v, Expr b);
EXPORT Interval operator*(Interval v, Expr b);
EXPORT Interval operator/(Interval v, Expr b);
EXPORT Interval operator%(Interval v, Expr b);


}
}

#endif
