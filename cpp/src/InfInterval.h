// Use this include file in the context of IR.h
#ifndef HALIDE_IR_H
# include "IR.h"
#endif

#ifndef HALIDE_INF_INTERVAL_H
#define HALIDE_INF_INTERVAL_H

/** \file
 * Define an InfInterval: an interval from min to max inclusive.
 * The bounds can never be undefined expressions, but they can be infinite.
 * This is in contrast to Interval which allows undefined bounds but not infinite.
 */

namespace Halide {

/** An InfInterval.  Includes all numbers from min to max inclusive. */
class InfInterval : public Internal::IntRange {
private:
    int extent; // Disable access to extent of IntRange object.
public:
    /** Constructor of full interval -infinity, infinity. */
    InfInterval() : IntRange(Mode_InfInterval) {}
    
    /** Constructor of full interval -infinity, infinity of specified type. */
    InfInterval(Type t) : IntRange(Mode_InfInterval, t) {}
    
    /** IntRange constructor checks that min and max cannot be undefined. */
    InfInterval(Expr _min, Expr _max) : IntRange(Mode_InfInterval, _min, _max) {}
    
    /** IntRange constructor accepts undefined min and/or max, and converts to infinity using specified type. */
    InfInterval(Type t, Expr _min, Expr _max) : IntRange(Mode_InfInterval, t, _min, _max) {}
    
    int imin();
    int imax();
};

/** C++ functions to manipulate an InfInterval object, returning a new InfInterval as the result */
EXPORT InfInterval operator+(InfInterval v, Expr b);
EXPORT InfInterval operator-(InfInterval v); // Unary negation
EXPORT InfInterval operator-(InfInterval v, Expr b);

/** Scaling an InfInterval up has two forms.
 * operator* multiplies the extremes by a constant.  This is the simple form.
 * For integer types, zoom results in b elements from each element of the original InfInterval.
 *    This represents zooming an image.  It is also the inverse of integer division because
 *    it gives you all the integers that, when divided using Halide floor int division, would
 *    result in the original InfInterval v.
 */
EXPORT InfInterval operator*(InfInterval v, Expr b);
EXPORT InfInterval zoom(InfInterval v, Expr b);

/** Scaling an InfInterval down has the following forms.
 * operator/ divides both values by a constant.  Note that v / k * k may result in
 *    values outside the original v, depending on the data type.
 * For integer types:
 * decimate selects every element of the InfInterval that is an integer multiple of b, and
 *    gives you the resulting down-scaled InfInterval.  This represents the pixel range
 *    that you get by decimating an image.  It also represents the inverse of integer
 *    multiply becaise it gives you all the integers that, when multiplied back up, would
 *    not overflow the original InfInterval v.
 * unzoom selects only those elements of the InfInterval that will be present if zoom
 *    is subsequently applied.  This is the appropriate way to reduce a main loop
 *    partition when the loop is split, because in this case we will be executing
 *    a subloop (equivalent to zoom).  Unzoom is the inverse of zoom.
 */
EXPORT InfInterval operator/(InfInterval v, Expr b);
EXPORT InfInterval decimate(InfInterval v, Expr b);
EXPORT InfInterval unzoom(InfInterval v, Expr b);

/** Operator% takes modulus of both extremes. */
EXPORT InfInterval operator%(InfInterval v, Expr b);

/** Inverse operators 
 * inverseAdd returns an interval such that adding b yields the original interval.  This is equivalent to subtraction.
 * inverseSub returns an interval such that subtracting b yields the original interval.  Equivalent to addition.
 * inverseMul returns an interval such that multiplication by b does not exceed
 *    the original interval.  This is equivalent to decimate.
 * inverseDiv returns an interval such that dividing it back down yields the original interval.
 *    Both zoom and operator* are candidates for this operation.
 *    Zoom is preferred if you are talking about indices, because it represents replicating each pixel n times.
 */
EXPORT InfInterval inverseAdd(InfInterval v, Expr b);
EXPORT InfInterval inverseSub(InfInterval v, Expr b);
EXPORT InfInterval inverseMul(InfInterval v, Expr b);

/** Operators on two InfIntervals */
EXPORT InfInterval operator+(InfInterval u, InfInterval v);
EXPORT InfInterval operator-(InfInterval u, InfInterval v);
EXPORT InfInterval operator*(InfInterval u, InfInterval v);
EXPORT InfInterval operator/(InfInterval u, InfInterval v);
EXPORT InfInterval operator%(InfInterval u, InfInterval v);
EXPORT InfInterval min(InfInterval u, InfInterval v);
EXPORT InfInterval max(InfInterval u, InfInterval v);
EXPORT InfInterval intersection(InfInterval u, InfInterval v);
EXPORT InfInterval infinterval_union(InfInterval u, InfInterval v);

/** Inverse operators
 * r = inverseAdd(u,v) is such that u = r + v
 * r = inverseSub(u,v) is such that u = r - v
 */
EXPORT InfInterval inverseAdd(InfInterval u, InfInterval v);
EXPORT InfInterval inverseSub(InfInterval u, InfInterval v);

namespace Internal {
EXPORT void infinterval_test();
}
} // end namespace Halide

#endif
