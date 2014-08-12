#include "IR.h"
#include "IROperator.h"
#include "Simplify.h"
#include "IRPrinter.h"
#include "IREquality.h"

namespace Halide {

namespace {
// Halide division implements floor division - i.e. it rounds the quotient towards -infinity.
// A side effect of this is that the remainder always has the same sign as the divisor.

// Ceiling division of a by positive b
// Examples: ceil(10,3) ==> 4. ceil(11,3) ==> 4.  ceil(12,3) ==> 4.
inline Expr pos_ceil(Expr a, Expr b) { return (a - 1) / b + 1; }
// Ceiling division of a by negative b
// Example: ceil(10,-3) ==> -3.  ceil(11,-3) ==> -3.  ceil(12,-3) ==> -4.
inline Expr neg_ceil(Expr a, Expr b) { return (a + 1) / b + 1; }
}

/** Implementation of manipulation of DomIntervals. */
/** Add a constant to an interval */
DomInterval operator+(DomInterval v, Expr b) { 
    return DomInterval(simplify(v.min + b), simplify(v.max + b), v.exact); 
}

/** Subtract a constant from an interval */
DomInterval operator-(DomInterval v, Expr b) { 
    return DomInterval(simplify(v.min - b), simplify(v.max - b), v.exact); 
}

/** Negate an interval */
DomInterval operator-(DomInterval v) { 
    return DomInterval(simplify(-v.max), simplify(-v.min), v.exact); 
}

/** Multiply an interval by a constant. If negative constant, the interval is flipped. */
DomInterval operator*(DomInterval v, Expr b) { 
    return DomInterval(simplify(select(b >= 0, v.min * b, v.max * b)), simplify(select(b >= 0, v.max * b, v.min * b)), v.exact); 
}

/** Divide an interval by a constant using Halide division (floor).  If negative constant, the interval is flipped */
DomInterval operator/(DomInterval v, Expr b) { 
    return DomInterval(simplify(select(b >= 0, v.min / b, v.max / b)), simplify(select(b >= 0, v.max / b, v.min / b)), v.exact); 
}

/** Multiply an interval by a constant in such a way that, if divided again, it will yield the original interval.
 * For integer intervals, each unit of the original interval is replicated b times.  This yields the largest
 * interval that divides down to cover the original interval.
 * Examples: zoom( I(3,5), 2) ==> I(6, 11)
 *    zoom( I(3,5), -2) ==> I(-11, -6)  because -11/-2 yields 5 with a remainder of -1 and -6/-2 yields -3.
 */
DomInterval zoom(DomInterval v, Expr b) { 
    if (b.type().is_float() || v.min.type().is_float() || v.max.type().is_float()) {
        // Result will be floating type, not integer.  zoom degenerates to multiply.
        return v * b;
    }

    // Multiplying an DomInterval (by an integer other than zero).
    // For positive b, the new DomInterval is min*b, max*b+(b-1).
    // For negative b, the new DomInterval is max*b+(b+1), min*b.
    
    Expr newmin = select(b >= 0, v.min * b, v.max * b + (b+1));
    Expr newmax = select(b >= 0, v.max * b + (b-1), v.min * b);
    return DomInterval(simplify(newmin), simplify(newmax), v.exact); 
}

/** Multiply an interval by a constant in such a way that, if divided again, it will yield the original interval.
 * This version of the operator yields the smallest upscaled interval, which happens to be the same as multiplication.
 */
DomInterval inverseDiv(DomInterval v, Expr b) { 
    return v * b;
}

/** Divide an interval by a constant in such a way that multiplying it up again will yield an interval
 * no larger than the original interval.
 * Examples: decimate( I(4,11), 3) ==> I(2,3)  because I(2,3) * 3 ==> (6, 9)
 *    decimate( I(4,11), -3) ==> I(-3,-2)  because I(-2,-3) * -3 ==> (6, 9)
 * It is called decimal because it gives you the valid pixel coordinates that you get if you decimate
 * an image (i.e. it gives you indices that you can multiply back up to access the pixels in the original image).
 */
DomInterval decimate(DomInterval v, Expr b) {
    if (b.type().is_float() || v.min.type().is_float() || v.max.type().is_float()) {
        // Result will be floating type, not integer.  decimate degenerates to division.
        return v / b;
    }
    
    // For positive b, the new DomInterval is ceil(min,b), max/b
    // For negative b, it is ceil(max,b), min/b

    Expr newmin = select(b >= 0, pos_ceil(v.min, b), neg_ceil(v.max, b));
    Expr newmax = select(b >= 0, v.max / b, v.min / b);
    return DomInterval(simplify(newmin), simplify(newmax), v.exact);
}

/** Divide an interval by a constant in such a way that zooming it up again would not
 * exceed the original interval.
 * Examples: unzoom( I(4,11), 3) ==> I(2,3)  because zoom( I(2,3), 3) ==> (6, 11)
 *           unzoom( I(4,10), 3) ==> I(2,2)  because zoom( I(2,2), 2) ==> (6,8)
 *           unzoom( I(4,11), -3) ==> I(-3,-2) because zoom( I(-3,-2), -3) ==> (6, 11)
 */
DomInterval unzoom(DomInterval v, Expr b) { 
    // Unzoom an DomInterval.  The semantics is that the
    // result should be such that if you zoom the DomInterval
    // up again then it will not exceed the original DomInterval.
    
    if (b.type().is_float() || v.min.type().is_float() || v.max.type().is_float()) {
        // Result will be floating type, not integer.  unzoom degenerates to division.
        return v / b;
    }

    // Since zoom expands the interval as much as possible, unzoom must shrink it if
    // zoom would expand it.
    // For positive b, the new interval is ceil(min,b), (max+1)/b-1
    // For negative b, the new interval is ceil(max,b), (min+1)/b+1
    Expr newmin = select(b >= 0, pos_ceil(v.min,b), neg_ceil(v.max,b));
    Expr newmax = select(b >= 0, (v.max + 1) / b - 1, (v.min - 1) / b - 1);
    return DomInterval(simplify(newmin), simplify(newmax), v.exact);
}

DomInterval inverseAdd(DomInterval v, Expr b) { return operator-(v,b); }
DomInterval inverseSub(DomInterval v, Expr b) { return operator+(v,b); }
DomInterval inverseSubA(Expr a, DomInterval v) { 
    Expr newmin = a - v.max;
    Expr newmax = a - v.min;
    return DomInterval(simplify(newmin), simplify(newmax), v.exact);
}

DomInterval inverseMul(DomInterval v, Expr b) { return decimate(v, b); }

/** Compute the interval u such that interval (u % b) is in interval v. */
DomInterval inverseMod(DomInterval v, Expr b) {
    // Note: b == 0 is not allowed - modulus against zero is not defined.  
    // If b == 0 (integer) then mod_interval will be set to (-1,1) which is meaningless.
    DomInterval mod_interval;
    if (b.type().is_float()) mod_interval = DomInterval(simplify(min(0,b)), simplify(max(0,b)), true);
    else mod_interval = DomInterval(simplify(min(0,b-1)), simplify(max(0,b+1)), true);
    // If v contains mod_interval then u is infinite.
    if (proved(v.min <= mod_interval.min) && proved(v.max >= mod_interval.max)) {
        return DomInterval(Internal::make_infinity(v.min.type(), -1), Internal::make_infinity(v.min.type(), +1), v.exact);
    }
    // Since v is not known to contain mod_interval, there are some values
    // of mod_interval that fall outside the target interval v.
    // In order to ensure that u % b is contained by v,
    // u must be the intersection of v and mod_interval.
    // i.e. Any value outside v but inside mod_interval cannot appear in u
    // because it does not appear in v and mod does not change it.
    // Also, any value outside mod_interval but inside v will be wrapped by
    // the application of modulus and at least one such wrapped value will
    // not be contained in v.
    return intersection(v, mod_interval);
}


DomInterval operator%(DomInterval u, Expr b) {
    // If we can prove that u is in the range of the modulus b then DomInterval u is the result. 
    // Otherwise, we simply that the interval from zero to b.
    if (proved(u.min >= 0) && proved(u.max < b)) {
        // u is a positive DomInterval and so is v.
        return u;
    } else if (proved(u.max <= 0) && proved(u.min > b)) {
        // u is a negative DomInterval and so is v.
        return u;
    } else {
        Expr zero = Internal::make_zero(u.min.type());
        // rmin/rmax are the limits that apply if v can take
        // respectively negative/positive values.
        Expr rmin, rmax;
        if (b.type().is_float()) {
            rmin = b;
        } else {
            rmin = b + 1;
        }
        if (b.type().is_float()) {
            rmax = b;
        } else {
            rmax = b - 1;
        }
        // The actual limits are further expanded to include zero.
        return DomInterval(simplify(min(rmin,zero)), simplify(max(rmax,zero)), u.exact);
    }
}

int DomInterval::imin() {
    int ival;
    assert (get_const_int(min, ival) && "Expected an integer value in the DomInterval");
    return ival;
}

int DomInterval::imax() {
    int ival;
    assert (get_const_int(max, ival) && "Expected an integer value in the DomInterval");
    return ival;
}


// ---------------------------------------
// Operators on pairs of DomIntervals.

DomInterval operator+(DomInterval u, DomInterval v) {
    return DomInterval(simplify(u.min + v.min), simplify(u.max + v.max), u.exact && v.exact);
}

DomInterval operator-(DomInterval u, DomInterval v) {
    return DomInterval(simplify(u.min - v.max), simplify(u.max - v.min), u.exact && v.exact);
}

DomInterval operator*(DomInterval u, DomInterval v) {
    // Special-case optimizations to generate less work for the constant-folder
    if (is_const(u.min) && equal(u.min, u.max)) {
        if (is_negative_const(u.min)) std::swap(v.min, v.max);
        return DomInterval(simplify(v.min * u.min), simplify(v.max * u.min), u.exact && v.exact);
    } else if (is_const(v.min) && equal(v.min, v.max)) {
        if (is_negative_const(v.min)) std::swap(u.min, u.max);
        return DomInterval(simplify(u.min * v.min), simplify(u.max * v.min), u.exact && v.exact);           
    } else {
        Expr a = u.min * v.min;
        Expr b = u.min * v.max;
        Expr c = u.max * v.min;
        Expr d = u.max * v.max;
        
        Expr rmin = min(min(a, b), min(c, d));
        Expr rmax = max(max(a, b), max(c, d));
        return DomInterval(simplify(rmin), simplify(rmax), u.exact && v.exact);
    }
}

DomInterval operator/(DomInterval u, DomInterval v) {
    if (is_const(v.min) && equal(v.min, v.max)) {
        if (is_negative_const(v.min)) std::swap(u.min, u.max); 
        return DomInterval(simplify(u.min / v.min), simplify(u.max / v.min), u.exact && v.exact);
    } else {
        // if we can't statically prove that the divisor can't span zero, then we're unbounded
        bool min_v_is_positive = proved(v.min > Internal::make_zero(v.min.type()));
        bool max_v_is_negative = proved(v.max < Internal::make_zero(v.max.type()));
        if (! min_v_is_positive && ! max_v_is_negative) {
            return DomInterval(Internal::make_infinity(v.min.type(), -1), Internal::make_infinity(v.max.type(), +1), u.exact && v.exact);
        } else {
            Expr a = u.min / v.min;
            Expr b = u.min / v.max;
            Expr c = u.max / v.min;
            Expr d = u.max / v.max;
            
            Expr rmin = min(min(a, b), min(c, d));
            Expr rmax = max(max(a, b), max(c, d));
            return DomInterval(simplify(rmin), simplify(rmax), u.exact && v.exact);
        }
    }
}

DomInterval operator%(DomInterval u, DomInterval v) {
    // If we can prove that u is in the range of the modulus v then DomInterval u is the result. 
    // Otherwise, we simply that the interval from zero to v.
    if (proved(u.min >= 0) && proved(u.max < v.min)) {
        // u is a positive DomInterval and so is v.
        return u;
    } else if (proved(u.max <= 0) && proved(u.min > v.max)) {
        // u is a negative DomInterval and so is v.
        return u;
    } else {
        Expr zero = Internal::make_zero(u.min.type());
        // rmin/rmax are the limits that apply if v can take
        // respectively negative/positive values.
        Expr rmin, rmax;
        if (v.min.type().is_float()) {
            rmin = v.min;
        } else {
            rmin = v.min + 1;
        }
        if (v.max.type().is_float()) {
            rmax = v.max;
        } else {
            rmax = v.max - 1;
        }
        // The actual limits are further expanded to include zero.
        return DomInterval(simplify(min(rmin,zero)), simplify(max(rmax,zero)), u.exact && v.exact);
    }
}

DomInterval max(DomInterval u, DomInterval v) {
    return DomInterval(simplify(max(u.min, v.min)), simplify(max(u.max, v.max)), u.exact && v.exact);
}

DomInterval min(DomInterval u, DomInterval v) {
    return DomInterval(simplify(min(u.min, v.min)), simplify(min(u.max, v.max)), u.exact && v.exact);
}

DomInterval intersection(DomInterval u, DomInterval v) {
    return DomInterval(simplify(max(u.min, v.min)), simplify(min(u.max, v.max)), u.exact && v.exact);
}

DomInterval interval_union(DomInterval u, DomInterval v) {
    return DomInterval(simplify(min(u.min, v.min)), simplify(max(u.max, v.max)), u.exact && v.exact);
}

DomInterval inverseAdd(DomInterval v, DomInterval k) {
    // Compute an interval such that adding the interval k always results
    // in an interval no bigger than v.
    // (v.min, v.max) = (r.min + k.min, r.max + k.max)
    // r.min = v.min - k.min
    // r.max = v.max - k .max
    return DomInterval(simplify(v.min - k.min), simplify(v.max - k.max), v.exact && k.exact);
}

DomInterval inverseSub(DomInterval v, DomInterval k) {
    // Compute an interval such that subtracting the interval k always results
    // in an interval no bigger than v.
    // (v.min, v.max) = (r.min - k.max, r.max - k.min)
    // r.min = v.min + k.max
    // r.max = v.max + k.min
    return DomInterval(simplify(v.min + k.max), simplify(v.max + k.min), v.exact && k.exact);
}


namespace Internal {

namespace {
    void check(DomInterval a, std::string op, Expr b, DomInterval result, DomInterval expected) {
        if (! equal(result.min, expected.min) || ! equal(result.max, expected.max) || result.exact != expected.exact) {
            std::cout << "DomInterval operations test failed: " << a << " " << op << " " << b << "\n";
            std::cout << "  expected: " << expected << "\n" << "  got: " << result << "\n";
            assert(0);
        }
    }
    
    void check(std::string op, DomInterval a, Expr b, DomInterval result, DomInterval expected) {
        if (! equal(result.min, expected.min) || ! equal(result.max, expected.max) || result.exact != expected.exact) {
            std::cout << "DomInterval operations test failed: " << op << "(" << a << ", " << b << ")\n";
            std::cout << "  expected: " << expected << "\n" << "  got: " << result << "\n";
            assert(0);
        }
    }
    
    void checkzoom(DomInterval a, int div) {
        DomInterval r = zoom(unzoom(a, div), div);
        int absdiv = div > 0 ? div : -div;
        if (r.imin() < a.imin() || r.imax() > a.imax() || r.imin() >= a.imin() + absdiv || r.imax() <= a.imax() - absdiv) {
            std::cout << "DomInterval unzoom and zoom test failed: zoom(unzoom(" << a << ", " << div << "), " << div << ")\n";
            std::cout << "  got: " << r << " from " << unzoom(a, div) << "from" << a << "\n";
            assert(0);
        }
    }
    
    void checkdecimate(DomInterval a, int div) {
        DomInterval r = decimate(a, div) * div;
        int absdiv = div > 0 ? div : -div;
        if (r.imin() < a.imin() || r.imax() > a.imax() || r.imin() >= a.imin() + absdiv || r.imax() <= a.imax() - absdiv) {
            std::cout << "DomInterval decimate and multiply test failed: decimate(" << a << ", " << div << ") * " << div << "\n";
            std::cout << "  got: " << r << " from " << unzoom(a, div) << "\n";
            assert(0);
        }
    }
    
    void checkzoomdiv(DomInterval a, int div) {
        // Check that if you zoom up an interval and then divide it you end
        // up with the original interval.
        // Also check that the new interval has every pixel multiplied div times.
        // Together, these two properties define zoom.
        DomInterval z = zoom(a,div);
        DomInterval r = z / div;
        int lz = z.imax() - z.imin() + 1;
        int la = a.imax() - a.imin() + 1;
        int absdiv = div > 0 ? div : -div;
        if (lz != la * absdiv) {
            std::cout << "DomInterval zoom test failed\n";
            std::cout << "  the original interval contains " << la << " integers\n";
            std::cout << "  the zoomed interval contains " << lz << " integers; expected " << la * absdiv << "\n";
            assert(0);
        }
        check(a, "zoomdiv", div, r, a);
    }
    
    void checkaddinv(DomInterval a, DomInterval b) {
        // Check that if you add two intervals and then apply inverse add
        // you get back the original
        DomInterval z = a + b;
        DomInterval r = inverseAdd(z, b);
        if (! equal(r.min, a.min) || ! equal(r.max, a.max)) {
            std::cout << "DomInterval inverseAdd test failed\n";
            std::cout << "  " << a << " + " << b << " --> " << z << "\n";
            std::cout << "  inverseAdd(" << z << ", " << b << ") --> " << r << "\n";
            assert(0);
        }
    }
    
    void checksubinv(DomInterval a, DomInterval b) {
        // Check that if you subtract two intervals and then apply inverse subtract
        // you get back the original
        DomInterval z = a - b;
        DomInterval r = inverseSub(z, b);
        if (! equal(r.min, a.min) || ! equal(r.max, a.max)) {
            std::cout << "DomInterval inverseSub test failed\n";
            std::cout << "  " << a << " - " << b << " --> " << z << "\n";
            std::cout << "  inverseSub(" << z << ", " << b << ") --> " << r << "\n";
            assert(0);
        }
    }
}

void dominterval_test () {
    DomInterval v1(5,1282,true);
    DomInterval v2(6,1281,true);
    DomInterval v3(7,1280,true); // Just outside the boundaries
    DomInterval v4(8,1279,true); // Exactly on the boundaries of multiples of 8
    DomInterval v5(9,1278,true); // Just inside the boundaries
    DomInterval v6(10,1277,true);
    DomInterval v7(11,1276,true);
    DomInterval va(1,159,true);
    DomInterval vb(-159,-1,true);
    
    // check unzoom against zoom
    checkzoom(v1, 8);
    checkzoom(v2, 8);
    checkzoom(v3, 8);
    checkzoom(v4, 8);
    checkzoom(v5, 8);
    checkzoom(v6, 8);
    checkzoom(v7, 8);
    checkzoom(v1, -8);
    checkzoom(v2, -8);
    checkzoom(v3, -8);
    checkzoom(v4, -8);
    checkzoom(v5, -8);
    checkzoom(v6, -8);
    checkzoom(v7, -8);
    checkzoomdiv(v1, 8);
    checkzoomdiv(v2, 8);
    checkzoomdiv(v3, 8);
    checkzoomdiv(v4, 8);
    checkzoomdiv(v5, 8);
    checkzoomdiv(v6, 8);
    checkzoomdiv(v7, 8);
    checkzoomdiv(v1, -8);
    checkzoomdiv(v2, -8);
    checkzoomdiv(v3, -8);
    checkzoomdiv(v4, -8);
    checkzoomdiv(v5, -8);
    checkzoomdiv(v6, -8);
    checkzoomdiv(v7, -8);
    checkdecimate(v1, 8);
    checkdecimate(v2, 8);
    checkdecimate(v3, 8);
    checkdecimate(v4, 8);
    checkdecimate(v5, 8);
    checkdecimate(v6, 8);
    checkdecimate(v7, 8);
    checkdecimate(v1, -8);
    checkdecimate(v2, -8);
    checkdecimate(v3, -8);
    checkdecimate(v4, -8);
    checkdecimate(v5, -8);
    checkdecimate(v6, -8);
    checkdecimate(v7, -8);
    checkaddinv(v1, v2);
    checkaddinv(v3, vb);
    checksubinv(v1, v2);
    checksubinv(v3, vb);
    std::cout << "DomInterval operations test passed\n";
}
}

// end namespace Halide
}

