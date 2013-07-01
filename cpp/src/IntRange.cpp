#include "IR.h"
#include "IROperator.h"
#include "IRPrinter.h"
#include "IREquality.h"
#include "Var.h"
#include "Simplify.h"

namespace Halide {
namespace Internal {

IntRange::IntRange(Mode _mode, bool _exact) : mode(_mode), exact(_exact) {
    // By default, min, max and extent are initialised to undefined expressions 
    if (mode == IntRange::Mode_DomInterval) {
        min = make_infinity(Int(32), -1);
        max = make_infinity(Int(32), +1);
    }
}
    
IntRange::IntRange(Mode _mode, Type t, bool _exact) : mode(_mode), exact(_exact) {
    // By default, min, max and extent are initialised to undefined expressions 
    if (mode == Mode_DomInterval) {
        min = make_infinity(t, -1);
        max = make_infinity(t, +1);
    }
}
    
IntRange::IntRange(Mode _mode, Expr _min, Expr _end, bool _exact) : mode(_mode), exact(_exact) {
    switch (_mode) {
    case Mode_DomInterval:
        assert_defined_same_type("DomInterval", _min, _end);
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

IntRange::IntRange(Mode _mode, Type t, Expr _min, Expr _end, bool _exact) : mode(_mode), exact(_exact) {
    switch (_mode) {
    case IntRange::Mode_DomInterval:
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
            Interval interval(min, max, exact);
            return interval;
        }
        
    case Mode_Range:
        {
            // Loss of information: If min or extent is not defined then max cannot
            // be defined. Conversion from Range to Interval and back
            // may lose the extent information.  But we cannot preserve the extent expression
            // because the user may change the exposed min and max expressions,
            // rendering any preserved extent expression incorrect.
            Expr emax;
            // Conversion designed to correspond well with Range(Interval).
            // extent = (max + 1) - min
            // so max = (extent + min) - 1
            // resolves to max = (((max + 1) - min) + min) - 1
            // which is easy to simplify.
            if (min.defined() && extent.defined()) emax = simplify((extent + min) - 1);
            Interval from_range(min, emax, exact);
            return from_range;
        }
        
    case Mode_DomInterval:
        {
            // Ambiguity of conversion: If the minimum is +infinity
            // or the maximum is -infinity, then that cannot be represented.
            // The DomInterval consists of a single infinity, but the
            // returned Interval consists of the entire real number line.
            Expr emin, emax;
            if (min.defined() && infinity_count(min) == 0) emin = min;
            if (max.defined() && infinity_count(max) == 0) emax = max;
            Interval from_inf(emin, emax, exact);
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
            if (min.defined() && max.defined()) eextent = simplify((max + 1) - min);
            Range from_interval(min, eextent, exact);
            return from_interval;
        }
        
    case Mode_Range:
        {
            Range range(min, extent, exact);
            return range;
        }
        
    case Mode_DomInterval:
        {
            // Ambiguity of conversion: If the minimum is +infinity
            // or the maximum is -infinity, that case cannot be represented
            Expr emin, emax, eextent;
            if (min.defined() && infinity_count(min) == 0) emin = min;
            if (max.defined() && infinity_count(max) == 0) emax = max;
            // Conversion as above, based on interval_to_range().
            if (emin.defined() && emax.defined()) eextent = simplify((emax + 1) - emin);
            Range from_dom(emin, eextent, exact);
            return from_dom;
        }
    
    default:
        assert(0 && "Unimplemented mode");
    }
}

// Convert IntRange to DomInterval without type specification
IntRange::operator DomInterval() {
    return DomInterval_type(Int(32));
}

// Convert IntRange to DomInterval with specified type t (applied as default)
DomInterval IntRange::DomInterval_type(Type t) {
    switch (mode) {
    case Mode_Interval:
        {
            // Interpretation of undefined min/max: corresponding infinity.
            // Use type parameter to constructor to convert undefined to infinity
            DomInterval from_interval(t, min, max, exact);
            return from_interval;
        }
        
    case Mode_Range:
        {
            // Loss of information: If min or extent is not defined then max cannot
            // be defined. 
            Expr emax;
            // Conversion designed to correspond well with Range(Interval).
            if (min.defined() && extent.defined()) emax = simplify((extent + min) - 1);
            // At this point, (min, emax) is an Interval.  Complete the conversion
            // to DomInterval but inherit type from extent if it is defined
            // because emax may be undefined even when extent is defined.
            if (extent.defined()) t = extent.type();
            DomInterval from_range(t, min, emax, exact);
            return from_range;
        }
        
    case Mode_DomInterval:
        {
            DomInterval dom(min, max, exact);
            return dom;
        }
    
    default:
        assert(0 && "Unimplemented mode");
    }
}


namespace {
void check_dom(DomInterval test, DomInterval expected) {
    if (! equal(test.min, expected.min) || ! equal(test.max, expected.max) || test.exact != expected.exact) {
        std::cerr << "IntRange test failed\n";
        std::cerr << "DomInterval check expected " << expected << "   got " << test << "\n";
        assert(0 && "IntRange test failed");
    }
}
void check_int(Interval test, Interval expected) {
    if (! equal(test.min, expected.min) || ! equal(test.max, expected.max) || test.exact != expected.exact) {
        std::cerr << "IntRange test failed\n";
        std::cerr << "Interval check expected " << expected << "   got " << test << "\n";
        assert(0 && "IntRange test failed");
    }
}
void check_range(Range test, Range expected) {
    if (! equal(test.min, expected.min) || ! equal(test.extent, expected.extent) || test.exact != expected.exact) {
        std::cerr << "IntRange test failed\n";
        std::cerr << "Range check expected " << expected << "   got " << test << "\n";
        assert(0 && "IntRange test failed");
    }
}
}

void intrange_test() {
    Var j("j"), k("k");
    // Test different ways of converting intervals.
    // We want the user to be able to pass any of the IntRange type intervals
    // to a function/method that accepts any of the other types.
    // Every derived class (Interval, Range, DomInterval) is an IntRange so
    // they can be used as an IntRange; and then there is conversion of
    // IntRange to each type so basically we can pass any of them to a
    // function/method that requires any other and it will work.
    DomInterval t1 = DomInterval(Interval(3, 6));
    check_dom(t1, DomInterval(3, 6, true));
    // An empty Interval() constructor returns undefined limits and exact=false.
    DomInterval t2 = DomInterval(Interval());
    check_dom(t2, DomInterval(Int(32), Expr(), Expr(), false));
    DomInterval t3 = DomInterval(Range(3,5));
    check_dom(t3, DomInterval(3, 7, true));
    DomInterval t4 = Interval(5,8);
    check_dom(t4, DomInterval(5, 8, true));
    check_dom(Interval(3,6), DomInterval(3,6,true));
    check_dom(Range(3,5), DomInterval(3,7,true));
    check_dom(DomInterval(), DomInterval(Int(32), Expr(), Expr(), true));
    check_dom(Interval(k, k+5), DomInterval(k,k+5,true));
    check_dom(Range(k,4), DomInterval(k,k+3,true));
    check_dom(Range(k,j), DomInterval(k,j+k+(-1),true));
    
    check_int(Interval(), Interval(DomInterval(Int(32), Expr(), Expr(), false)));
    check_int(Range(3,5), Interval(3,7));
    check_int(Range(Interval(j,j+k)), Interval(j,k+j));
    check_int(Range(Interval(3,10)), Interval(3,10));
    
    check_range(Interval(j,k), Range(j,k-j+1));
    check_range(Interval(Range(j,k)), Range(j,k));
    check_range(DomInterval(j,50,true), Range(j,51-j));
}

} // end namespace Internal

} // end namespace Halide
