// This include file must be included in the manner that IR.h includes it.
// Do not include this file directly: include IR.h instead.
#ifndef HALIDE_IR_H
# include HALIDE_IR_H
#endif

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
class InfInterval;

namespace Internal {
// A base class for various type of interval/range objects.
class IntRange {
protected:
    typedef enum { Mode_Interval, Mode_Range, Mode_InfInterval } Mode;
    Mode mode;
    
    /** Protected constructors for use by derived classes only.
     * Construct in appropriate mode, with full interval of reals. */
    IntRange(Mode _mode);
    
    /** Construct in appropriate mode, with type specified.
     * Type is only relevant to Mode_InfInterval because of infinities. */
    IntRange(Mode _mode, Type t);
    
    /** Construct with specified expressions for min and end.
     * Use the mode to determine meaning of _end parameter.
     * Infinities are not allowed for Mode_Interval or Mode_Range;
     * undefined exprs are not allowed for Mode_InfInterval. */
    IntRange(Mode _mode, Expr _min, Expr _end);
    
    /** Construct with specified expressions for min and end.
     * Use the mode to determine meaning of _end parameter.
     * Use type parameter to determine type of infinities for Mode_InfInterval
     * if both ends are undefined.  Undefined is converted to 
     * infinities for Mod_InfInterval.  Other modes still do not
     * permit infinities - you have to convert from InfInterval. */
    IntRange(Mode _mode, Type t, Expr _min, Expr _end);
    
public:
    Expr min, max, extent;
    IntRange() : mode(Mode_Interval) {}
    
    /** Conversion operators produce the individual types from IntRange base type. */
    operator InfInterval();
    operator Range();
    operator Interval();
    
    /** Conversion method that allows specifying the Halide Type to use for infinity. */
    InfInterval type_InfInterval(Type t);
};

} // end namespace Internal

/** Range: A single-dimensional span. Includes all numbers between min and
 * (min + extent - 1) */
class Range : public Internal::IntRange{
    int max; // Prevent access to IntRange.max.
public:
    Range() : Internal::IntRange() {}
    Range(Expr min, Expr extent) : Internal::IntRange(IntRange::Mode_Range, min, extent) {
        // Assumes that both min and extent are defined.
        assert(min.type() == extent.type() && "Region min and extent must have same type");
    }
    Range(Internal::IntRange ir);
};

} // end namespace Halide

#include "InfInterval.h"
#include "Interval.h"

#endif
