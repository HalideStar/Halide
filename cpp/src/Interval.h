#ifndef HALIDE_INTERVAL_H
#define HALIDE_INTERVAL_H

/** \file
 * Define an Interval: a range from min to max inclusive.
 */

namespace Halide {

/** An interval.  Includes all numbers from min to max inclusive. */
struct Interval {
    Expr min, max;
    Interval(Expr min, Expr max) : min(min), max(max) {}
    Interval() {}
    
    int imin();
    int imax();
};


/** C++ functions to manipulate an Interval object, returning a new Interval as the result */
EXPORT Interval operator+(Interval v, Expr b);
EXPORT Interval operator-(Interval v); // Unary negation
EXPORT Interval operator-(Interval v, Expr b);

/** Scaling an interval up has two forms.
 * operator* multiplies the extremes by a constant.  This is the simple form.
 * For integer types, zoom results in b elements from each element of the original interval.
 *    This represents zooming an image.  It is also the inverse of integer division because
 *    it gives you all the integers that, when divided using Halide floor int division, would
 *    result in the original interval v.
 */
EXPORT Interval operator*(Interval v, Expr b);
EXPORT Interval zoom(Interval v, Expr b);

/** Scaling an interval down has the following forms.
 * operator/ divides both values by a constant.  Note that v / k * k may result in
 *    values outside the original v, depending on the data type.
 * For integet types:
 * decimate selects every element of the interval that is an integer multiple of b, and
 *    gives you the resulting down-scaled interval.  This represents the pixel range
 *    that you get by decimating an image.  It also represents the inverse of integer
 *    multiply becaise it gives you all the integers that, when multiplied back up, would
 *    not overflow the original interval v.
 * unzoom selects only those elements of the interval that will be present if zoom
 *    is subsequently applied.  This is the appropriate way to reduce a main loop
 *    partition when the loop is split, because in this case we will be executing
 *    a subloop (equivalent to zoom).  Unzoom is the inverse of zoom.
 */
EXPORT Interval operator/(Interval v, Expr b);
EXPORT Interval decimate(Interval v, Expr b);
EXPORT Interval unzoom(Interval v, Expr b);
EXPORT Interval operator%(Interval v, Expr b);
EXPORT Interval intersection(Interval u, Interval v);

/** Operators on two intervals */
EXPORT Interval operator+(Interval u, Interval v);
EXPORT Interval operator-(Interval u, Interval v);
EXPORT Interval operator*(Interval u, Interval v);
EXPORT Interval operator/(Interval u, Interval v);
EXPORT Interval min(Interval u, Interval v);
EXPORT Interval max(Interval u, Interval v);

namespace Internal {
EXPORT void interval_test();
}
}

#endif
