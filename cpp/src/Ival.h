#ifndef HALIDE_IVAL_H
#define HALIDE_IVAL_H

#include "Interval.h"

/** \file
 * Define an Ival: an interval from min to max inclusive.
 * The bounds can never be undefined expressions, but they can be infinite.
 * This is in contrast to Interval which allows undefined bounds but not infinite.
 */

namespace Halide {

/** An Ival.  Includes all numbers from min to max inclusive. */
class Ival {
public:
    Expr min, max;
    
    // Constructor ensures that min and max cannot be undefined.
    Ival(Expr _min, Expr _max) : min(_min), max(_max) { assert(min.defined() && max.defined() && "Ival of undefined"); }
    Ival();
    
    Ival(Interval i);
    
    int imin();
    int imax();
};

/** C++ functions to manipulate an Ival object, returning a new Ival as the result */
EXPORT Ival operator+(Ival v, Expr b);
EXPORT Ival operator-(Ival v); // Unary negation
EXPORT Ival operator-(Ival v, Expr b);

/** Scaling an Ival up has two forms.
 * operator* multiplies the extremes by a constant.  This is the simple form.
 * For integer types, zoom results in b elements from each element of the original Ival.
 *    This represents zooming an image.  It is also the inverse of integer division because
 *    it gives you all the integers that, when divided using Halide floor int division, would
 *    result in the original Ival v.
 */
EXPORT Ival operator*(Ival v, Expr b);
EXPORT Ival zoom(Ival v, Expr b);

/** Scaling an Ival down has the following forms.
 * operator/ divides both values by a constant.  Note that v / k * k may result in
 *    values outside the original v, depending on the data type.
 * For integet types:
 * decimate selects every element of the Ival that is an integer multiple of b, and
 *    gives you the resulting down-scaled Ival.  This represents the pixel range
 *    that you get by decimating an image.  It also represents the inverse of integer
 *    multiply becaise it gives you all the integers that, when multiplied back up, would
 *    not overflow the original Ival v.
 * unzoom selects only those elements of the Ival that will be present if zoom
 *    is subsequently applied.  This is the appropriate way to reduce a main loop
 *    partition when the loop is split, because in this case we will be executing
 *    a subloop (equivalent to zoom).  Unzoom is the inverse of zoom.
 */
EXPORT Ival operator/(Ival v, Expr b);
EXPORT Ival decimate(Ival v, Expr b);
EXPORT Ival unzoom(Ival v, Expr b);
EXPORT Ival operator%(Ival v, Expr b);
EXPORT Ival intersection(Ival u, Ival v);

/** Operators on two Ivals */
EXPORT Ival operator+(Ival u, Ival v);
EXPORT Ival operator-(Ival u, Ival v);
EXPORT Ival operator*(Ival u, Ival v);
EXPORT Ival operator/(Ival u, Ival v);
EXPORT Ival operator%(Ival u, Ival v);
EXPORT Ival min(Ival u, Ival v);
EXPORT Ival max(Ival u, Ival v);
EXPORT Ival intersection(Ival u, Ival v);
EXPORT Ival ival_union(Ival u, Ival v);

namespace Internal {
EXPORT void ival_test();
}
}

#endif
