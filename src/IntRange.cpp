#include "IR.h"
#include "IROperator.h"

namespace Halide {
namespace Internal {

IntRange::IntRange(Mode _mode) : mode(_mode) {
    if (mode == IntRange::Mode_InfInterval) {
        min = make_infinity(Int(32), -1);
        max = make_infinity(Int(32), +1);
    }
}
    
IntRange::IntRange(Mode _mode, Type t) : mode(_mode) {
    if (mode == Mode_InfInterval) {
        min = make_infinity(t, -1);
        max = make_infinity(t, +1);
    }
}
    
IntRange::IntRange(Mode _mode, Expr _min, Expr _end) : mode(_mode) {
    switch (_mode) {
    case Mode_InfInterval:
        assert_defined_same_type("InfInterval", _min, _end);
        min = _min;
        max = _end;
        extent = Expr();
        break;
    case Mode_Interval:
        assert((!_min.defined() || infinity_count(_min) == 0) && "Infinity not permitted in Interval");
        assert((!_end.defined() || infinity_count(_end) == 0) && "Infinity not permitted in Interval");
        assert_same_type("Interval", _min, _end);
        min = _min;
        max = _end;
        extent = Expr();
        break;
    case Mode_Range:
        assert((!_min.defined() || infinity_count(_min) == 0) && "Infinity not permitted in Range");
        assert((!_end.defined() || infinity_count(_end) == 0) && "Infinity not permitted in Range");
        assert_same_type("Range", _min, _end);
        min = _min;
        max = Expr();
        extent = _end;
        break;
    }
}

IntRange::IntRange(Mode _mode, Type t, Expr _min, Expr _end) : mode(_mode) {
    switch (_mode) {
    case IntRange::Mode_InfInterval:
        // Conversion of undefined to infinity - because this flavour of
        // constructor implies this conversion.
        if (! _min.defined()) {
            if (_end.defined()) _min = make_infinity(_end.type(), -1);
            else _min = make_infinity(t, -1);
        }
        if (! _end.defined()) {
            if (_min.defined()) _end = make_infinity(_min.type(), +1);
            else _end = make_infinity(t, +1);
        }
        min = _min;
        max = _end;
        extent = Expr();
        break;
        
    case IntRange::Mode_Interval:
        assert((!_min.defined() || infinity_count(_min) == 0) && "Infinity not permitted in Interval");
        assert((!_end.defined() || infinity_count(_end) == 0) && "Infinity not permitted in Interval");
        assert_same_type("Interval", _min, _end);
        min = _min;
        max = _end;
        extent = Expr();
        break;
        
    case IntRange::Mode_Range:
        assert((!_min.defined() || infinity_count(_min) == 0) && "Infinity not permitted in Range");
        assert((!_end.defined() || infinity_count(_end) == 0) && "Infinity not permitted in Range");
        assert_same_type("Range", _min, _end);
        min = _min;
        max = Expr();
        extent = _end;
        break;
    }
}

/** Conversion operators */

IntRange::operator Interval() {
    switch (mode) {
    case Mode_Interval:
        {
            // Trivial case of matching mode.
            Interval interval(min, max);
            return interval;
        }
        
    case Mode_Range:
        {
            // Loss of information: If min or extent is not defined then max cannot
            // be defined. Conversion from Range to Interval and back
            // may lose the extent information.  But we cannot preserve it
            // because the user may change the exposed min and max expressions,
            // rendering any preserved extent expression incorrect.
            Expr emax;
            // Conversion designed to correspond well with Range(Interval).
            // extent = (max + 1) - min
            // so max = (extent + min) - 1
            // resolves to max = (((max + 1) - min) + min) - 1
            // which is easy to simplify.
            if (min.defined() && extent.defined()) emax = (extent + min) - 1;
            Interval from_range(min, emax);
            return from_range;
        }
        
    case Mode_InfInterval:
        {
            // Ambiguity of conversion: If the minimum is +infinity
            // or the maximum is -infinity, then that cannot be represented.
            // The InfInterval consists of a single infinity, but the
            // returned Interval consists of the entire real number line.
            Expr emin = min, emax = max;
            if (infinity_count(emin) != 0) emin = Expr();
            if (infinity_count(emax) != 0) emax = Expr();
            Interval from_inf(emin, emax);
            return from_inf;
        }
    
    default:
        assert(0 && "Unimplemented mode");
    }
}

IntRange::operator Range() {
    switch (mode) {
    case Mode_Interval:
        {
            // Loss of information: Conversion from interval to range
            // and back will lose the max if min is undefined.
            Expr eextent;
            // Conversion based on interval_to_range().
            if (min.defined() && max.defined()) eextent = (max + 1) - min;
            Range from_interval(min, eextent);
            return from_interval;
        }
        
    case Mode_Range:
        {
            Range range(min, extent);
            return range;
        }
        
    case Mode_InfInterval:
        {
            // Ambiguity of conversion: If the minimum is +infinity
            // or the maximum is -infinity, that case cannot be represented
            Expr emin, emax, eextent;
            if (infinity_count(emin) == 0) emin = min;
            if (infinity_count(emax) == 0) emax = max;
            // Conversion as above, based on interval_to_range().
            if (emin.defined() && emax.defined()) eextent = (emax + 1) - emin;
            Range from_inf(emin, eextent);
            return from_inf;
        }
    
    default:
        assert(0 && "Unimplemented mode");
    }
}

// Convert IntRange to InfInterval without type specification
IntRange::operator InfInterval() {
    return type_InfInterval(Int(32));
}

// Convert IntRange to InfInterval with specified type t (applied as default)
InfInterval IntRange::type_InfInterval(Type t) {
    switch (mode) {
    case Mode_Interval:
        {
            // Interpretation of undefined min/max: corresponding infinity.
            // Use type parameter to constructor to convert undefined to infinity
            InfInterval from_interval(t, min, max);
            return from_interval;
        }
        
    case Mode_Range:
        {
            // Loss of information: If min or extent is not defined then max cannot
            // be defined. 
            Expr emax;
            // Conversion designed to correspond well with Range(Interval).
            if (min.defined() && extent.defined()) emax = (extent + min) - 1;
            // At this point, (min, emax) is an Interval.  Complete the conversion
            // to InfInterval but inherit type from extent if it is defined
            // because emax may be undefined even when extent is defined.
            if (extent.defined()) t = extent.type();
            InfInterval from_range(t, min, emax);
            return from_range;
        }
        
    case Mode_InfInterval:
        {
            InfInterval inf(min, max);
            return inf;
        }
    
    default:
        assert(0 && "Unimplemented mode");
    }
}

} // end namespace Internal

} // end namespace Halide
