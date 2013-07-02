// Use this include file in the context of IR.h
#ifndef HALIDE_IR_H
# include "IR.h"
#endif

#ifndef HALIDE_INF_INTERVAL_H
#define HALIDE_INF_INTERVAL_H

/** \file
 * Define an DomInterval: an interval from min to max inclusive.
 * The bounds can never be undefined expressions, but they can be infinite.
 * This is in contrast to Interval which allows undefined bounds but not infinite.
 * Also, a DomInterval can be flagged as exact or not.
 */

namespace Halide {

/** An DomInterval.  Includes all numbers from min to max inclusive. */
class DomInterval : public Internal::IntRange {
private:
    int extent; // Disable access to extent of IntRange object.
public:
    /** Constructor of full interval -infinity, infinity. */
    DomInterval() : IntRange(Mode_DomInterval, true) {}
    
    /** Constructor of full interval -infinity, infinity of specified type. */
    DomInterval(Type t, bool _exact) : IntRange(Mode_DomInterval, t, _exact) {}
    
    /** IntRange constructor checks that min and max cannot be undefined. */
    DomInterval(Expr _min, Expr _max, bool _exact) : IntRange(Mode_DomInterval, _min, _max, _exact) {}
    
    /** IntRange constructor accepts undefined min and/or max, and converts to infinity using specified type. */
    DomInterval(Type t, Expr _min, Expr _max, bool _exact) : IntRange(Mode_DomInterval, t, _min, _max, _exact) {}
    
    int imin();
    int imax();
};

/** C++ functions to manipulate an DomInterval object, returning a new DomInterval as the result */
EXPORT DomInterval operator+(DomInterval v, Expr b);
EXPORT DomInterval operator-(DomInterval v); // Unary negation
EXPORT DomInterval operator-(DomInterval v, Expr b);

/** Scaling an DomInterval up has two forms.
 * operator* multiplies the extremes by a constant.  This is the simple form.
 * For integer types, zoom results in b elements from each element of the original DomInterval.
 *    This represents zooming an image.  It is also the inverse of integer division because
 *    it gives you all the integers that, when divided using Halide floor int division, would
 *    result in the original DomInterval v.
 */
EXPORT DomInterval operator*(DomInterval v, Expr b);
EXPORT DomInterval zoom(DomInterval v, Expr b);

/** Scaling an DomInterval down has the following forms.
 * operator/ divides both values by a constant.  Note that v / k * k may result in
 *    values outside the original v, depending on the data type.
 * For integer types:
 * decimate selects every element of the DomInterval that is an integer multiple of b, and
 *    gives you the resulting down-scaled DomInterval.  This represents the pixel range
 *    that you get by decimating an image.  It also represents the inverse of integer
 *    multiply because it gives you all the integers that, when multiplied back up, would
 *    not overflow the original DomInterval v.
 * unzoom selects only those elements of the DomInterval that will be present if zoom
 *    is subsequently applied.  This is the appropriate way to reduce a main loop
 *    partition when the loop is split, because in this case we will be executing
 *    a subloop (equivalent to zoom).  Unzoom is the inverse of zoom.
 */
EXPORT DomInterval operator/(DomInterval v, Expr b);
EXPORT DomInterval decimate(DomInterval v, Expr b);
EXPORT DomInterval unzoom(DomInterval v, Expr b);

/** Operator% is complicated */
EXPORT DomInterval operator%(DomInterval v, Expr b);

/** Inverse operators 
 * inverseAdd returns an interval such that adding b yields the original interval.  This is equivalent to subtraction.
 * inverseSub returns an interval such that subtracting b yields the original interval.  Equivalent to addition.
 * inverseSubA(a,v) return v such that a - v yields the original interval.
 * inverseMul returns an interval such that multiplication by b does not exceed
 *    the original interval.  This is equivalent to decimate.
 * inverse div: returns an interval such that dividing it back down yields the original interval.
 *    Both zoom and operator* are candidates for this operation.
 *    Zoom is preferred if you are talking about indices, because it represents replicating each pixel n times.
 *    Zoom returns the largest solution interval while operator* returns the smallest solution interval.
 * inverseMod: returns the largest interval such that taking modulus yields the original interval.
 */
EXPORT DomInterval inverseAdd(DomInterval v, Expr b);
EXPORT DomInterval inverseSub(DomInterval v, Expr b);
EXPORT DomInterval inverseSubA(Expr a, DomInterval v);
EXPORT DomInterval inverseMul(DomInterval v, Expr b);
EXPORT DomInterval inverseMod(DomInterval v, Expr b);

/** Operators on two DomIntervals */
EXPORT DomInterval operator+(DomInterval u, DomInterval v);
EXPORT DomInterval operator-(DomInterval u, DomInterval v);
EXPORT DomInterval operator*(DomInterval u, DomInterval v);
EXPORT DomInterval operator/(DomInterval u, DomInterval v);
EXPORT DomInterval operator%(DomInterval u, DomInterval v);
EXPORT DomInterval min(DomInterval u, DomInterval v);
EXPORT DomInterval max(DomInterval u, DomInterval v);
EXPORT DomInterval intersection(DomInterval u, DomInterval v);
EXPORT DomInterval interval_union(DomInterval u, DomInterval v);

/** Inverse operators
 * r = inverseAdd(u,v) is such that u = r + v
 * r = inverseSub(u,v) is such that u = r - v
 */
EXPORT DomInterval inverseAdd(DomInterval u, DomInterval v);
EXPORT DomInterval inverseSub(DomInterval u, DomInterval v);

namespace Internal {
EXPORT void dominterval_test();
}
} // end namespace Halide

#endif
