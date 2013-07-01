// Include this file via IR.h
#include "IR.h"

#ifndef HALIDE_INT_RANGE_H
#define HALIDE_INT_RANGE_H

/** \file
 * A base class to unify Interval and Range.
 * By deriving both Interval and Range from a common base class,
 * they can be used interchangeably for user interface.
 */

namespace Halide {

class Interval;
class Range;
class DomInterval;

namespace Internal {
// A base class for various type of interval/range objects.
class IntRange {
protected:
    typedef enum { Mode_Interval, Mode_Range, Mode_DomInterval } Mode;
    Mode mode;
    
    /** Protected constructors for use by derived classes only.
     * Construct in appropriate mode, with full interval of reals. */
    IntRange(Mode _mode, bool _exact);
    
    /** Construct in appropriate mode, with type specified.
     * Type is only relevant to Mode_DomInterval because of infinities. */
    IntRange(Mode _mode, Type t, bool _exact);
    
    /** Construct with specified expressions for min and end.
     * Use the mode to determine meaning of _end parameter.
     * Infinities are not allowed for Mode_Interval or Mode_Range;
     * undefined exprs are not allowed for Mode_DomInterval. */
    IntRange(Mode _mode, Expr _min, Expr _end, bool _exact);
    
    /** Construct with specified expressions for min and end.
     * Use the mode to determine meaning of _end parameter.
     * Use type parameter to determine type of infinities for Mode_DomInterval
     * if both ends are undefined.  Undefined is converted to 
     * infinities for Mod_DomInterval.  Other modes still do not
     * permit infinities - you have to convert from DomInterval. */
    IntRange(Mode _mode, Type t, Expr _min, Expr _end, bool _exact);
    
public:
    Expr min, max, extent;
    
    /** exact: If true, the interval is exact; if false it is an approximation.
     * This flag is needed by DomInterval but for consistency of semantics it can
     * be used by other forms of interval also. Exact should only be set true
     * when the source of information is ablt to certify that the interval is exact. */
    bool exact;
    
    /** Public constructor to support construction of arrays etc. */
    IntRange() : mode(Mode_Interval), exact(false) {}
    
    /** Conversion operators produce the individual types from IntRange base type. */
    operator DomInterval();
    operator Range();
    operator Interval();
    
    /** Conversion method that allows specifying the Halide Type to use for infinity conversion. */
    DomInterval DomInterval_type(Type t);
};

void intrange_test();

} // end namespace Internal

/** Range: A single-dimensional span. Includes all numbers between min and
 * (min + extent - 1) */
class Range : public Internal::IntRange{
    int max; // Prevent access to IntRange.max.
public:
    Range() : Internal::IntRange() {}
    Range(Expr min, Expr extent, bool exact = true) : Internal::IntRange(IntRange::Mode_Range, min, extent, exact) {
        // Assumes that both min and extent are defined.
        assert(min.type() == extent.type() && "Region min and extent must have same type");
    }
    //Range(Internal::IntRange ir);  // Use operator Range() to get Range from IntRange
};

} // end namespace Halide

#include "DomInterval.h"
#include "Interval.h"

#endif
