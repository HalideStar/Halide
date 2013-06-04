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

/** Implementation of manipulation of InfIntervals. */
/** Add a constant to an interval */
InfInterval operator+(InfInterval v, Expr b) { 
    return InfInterval(simplify(v.min + b), simplify(v.max + b)); 
}

/** Subtract a constant from an interval */
InfInterval operator-(InfInterval v, Expr b) { 
    return InfInterval(simplify(v.min - b), simplify(v.max - b)); 
}

/** Negate an interval */
InfInterval operator-(InfInterval v) { 
    return InfInterval(simplify(-v.max), simplify(-v.min)); 
}

/** Multiply an interval by a constant. If negative constant, the interval is flipped. */
InfInterval operator*(InfInterval v, Expr b) { 
    return InfInterval(simplify(select(b >= 0, v.min * b, v.max * b)), simplify(select(b >= 0, v.max * b, v.min * b))); 
}

/** Divide an interval by a constant using Halide division (floor).  If negative constant, the interval is flipped */
InfInterval operator/(InfInterval v, Expr b) { 
    return InfInterval(simplify(select(b >= 0, v.min / b, v.max / b)), simplify(select(b >= 0, v.max / b, v.min / b))); 
}

/** Multiply an interval by a constant in such a way that, if divided again, it will yield the original interval.
 * For integer intervals, each unit of the original interval is replicated b times.  This yields the largest
 * interval that divides down to cover the original interval.
 * Examples: zoom( I(3,5), 2) ==> I(6, 11)
 *    zoom( I(3,5), -2) ==> I(-11, -6)  because -11/-2 yields 5 with a remainder of -1 and -6/-2 yields -3.
 */
InfInterval zoom(InfInterval v, Expr b) { 
    if (b.type().is_float() || v.min.type().is_float() || v.max.type().is_float()) {
        // Result will be floating type, not integer.  zoom degenerates to multiply.
        return v * b;
    }

    // Multiplying an InfInterval (by an integer other than zero).
    // For positive b, the new InfInterval is min*b, max*b+(b-1).
    // For negative b, the new InfInterval is max*b+(b+1), min*b.
    
    Expr newmin = select(b >= 0, v.min * b, v.max * b + (b+1));
    Expr newmax = select(b >= 0, v.max * b + (b-1), v.min * b);
    return InfInterval(simplify(newmin), simplify(newmax)); 
}

/** Multiply an interval by a constant in such a way that, if divided again, it will yield the original interval.
 * This version of the operator yields the smallest upscaled interval, which happens to be the same as multiplication.
 */
InfInterval inverseDiv(InfInterval v, Expr b) { 
    return v * b;
}

/** Divide an interval by a constant in such a way that multiplying it up again will yield an interval
 * no larger than the original interval.
 * Examples: decimate( I(4,11), 3) ==> I(2,3)  because I(2,3) * 3 ==> (6, 9)
 *    decimate( I(4,11), -3) ==> I(-3,-2)  because I(-2,-3) * -3 ==> (6, 9)
 * It is called decimal because it gives you the valid pixel coordinates that you get if you decimate
 * an image (i.e. it gives you indices that you can multiply back up to access the pixels in the original image).
 */
InfInterval decimate(InfInterval v, Expr b) {
    if (b.type().is_float() || v.min.type().is_float() || v.max.type().is_float()) {
        // Result will be floating type, not integer.  decimate degenerates to division.
        return v / b;
    }
    
    // For positive b, the new InfInterval is ceil(min,b), max/b
    // For negative b, it is ceil(max,b), min/b

    Expr newmin = select(b >= 0, pos_ceil(v.min, b), neg_ceil(v.max, b));
    Expr newmax = select(b >= 0, v.max / b, v.min / b);
    return InfInterval(simplify(newmin), simplify(newmax));
}

/** Divide an interval by a constant in such a way that zooming it up again would not
 * exceed the original interval.
 * Examples: unzoom( I(4,11), 3) ==> I(2,3)  because zoom( I(2,3), 3) ==> (6, 11)
 *           unzoom( I(4,10), 3) ==> I(2,2)  because zoom( I(2,2), 2) ==> (6,8)
 *           unzoom( I(4,11), -3) ==> I(-3,-2) because zoom( I(-3,-2), -3) ==> (6, 11)
 */
InfInterval unzoom(InfInterval v, Expr b) { 
    // Unzoom an InfInterval.  The semantics is that the
    // result should be such that if you zoom the InfInterval
    // up again then it will not exceed the original InfInterval.
    
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
    return InfInterval(simplify(newmin), simplify(newmax));
}

InfInterval inverseAdd(InfInterval v, Expr b) { return operator-(v,b); }
InfInterval inverseSub(InfInterval v, Expr b) { return operator+(v,b); }
InfInterval inverseMul(InfInterval v, Expr b) { return decimate(v, b); }

# if 0
InfInterval operator%(InfInterval v, Expr b) { 
    return InfInterval(simplify(v.min % b), simplify(v.max % b)); 
}
# endif

int InfInterval::imin() {
    int ival;
    assert (get_const_int(min, ival) && "Expected an integer value in the InfInterval");
    return ival;
}

int InfInterval::imax() {
    int ival;
    assert (get_const_int(max, ival) && "Expected an integer value in the InfInterval");
    return ival;
}


// ---------------------------------------
// Operators on pairs of InfIntervals.

InfInterval operator+(InfInterval u, InfInterval v) {
    return InfInterval(simplify(u.min + v.min), simplify(u.max + v.max));
}

InfInterval operator-(InfInterval u, InfInterval v) {
    return InfInterval(simplify(u.min - v.max), simplify(u.max - v.min));
}

InfInterval operator*(InfInterval u, InfInterval v) {
    // Special-case optimizations to generate less work for the constant-folder
    if (is_const(u.min) && equal(u.min, u.max)) {
        if (is_negative_const(u.min)) std::swap(v.min, v.max);
        return InfInterval(simplify(v.min * u.min), simplify(v.max * u.min));
    } else if (is_const(v.min) && equal(v.min, v.max)) {
        if (is_negative_const(v.min)) std::swap(u.min, u.max);
        return InfInterval(simplify(u.min * v.min), simplify(u.max * v.min));           
    } else {
        Expr a = u.min * v.min;
        Expr b = u.min * v.max;
        Expr c = u.max * v.min;
        Expr d = u.max * v.max;
        
        Expr rmin = min(min(a, b), min(c, d));
        Expr rmax = max(max(a, b), max(c, d));
        return InfInterval(simplify(rmin), simplify(rmax));
    }
}

InfInterval operator/(InfInterval u, InfInterval v) {
    if (is_const(v.min) && equal(v.min, v.max)) {
        if (is_negative_const(v.min)) std::swap(u.min, u.max); 
        return InfInterval(simplify(u.min / v.min), simplify(u.max / v.min));
    } else {
        // if we can't statically prove that the divisor can't span zero, then we're unbounded
        bool min_v_is_positive = proved(v.min > Internal::make_zero(v.min.type()));
        bool max_v_is_negative = proved(v.max < Internal::make_zero(v.max.type()));
        if (! min_v_is_positive && ! max_v_is_negative) {
            return InfInterval(Expr(new Internal::Infinity(-1)), Expr(new Internal::Infinity(1)));
        } else {
            Expr a = u.min / v.min;
            Expr b = u.min / v.max;
            Expr c = u.max / v.min;
            Expr d = u.max / v.max;
            
            Expr rmin = min(min(a, b), min(c, d));
            Expr rmax = max(max(a, b), max(c, d));
            return InfInterval(simplify(rmin), simplify(rmax));
        }
    }
}

InfInterval operator%(InfInterval u, InfInterval v) {
    // If we can prove that u is in the range of mod v then InfInterval u is the result. 
    if (proved(u.min >= 0) && proved(u.max < v.min)) {
        // u is a positive InfInterval and so is v.
        return u;
    } else if (proved(u.max <= 0) && proved(u.min > v.max)) {
        // u is a negative InfInterval and so is v.
        return u;
    } else {
        Expr rmin = Internal::make_zero(u.min.type());
        Expr rmax;
        if (v.max.type().is_float()) {
            rmax = v.max;
        } else {
            rmax = v.max - 1;
        }
        return InfInterval(simplify(rmin), simplify(rmax));
    }
}

InfInterval max(InfInterval u, InfInterval v) {
    return InfInterval(simplify(max(u.min, v.min)), simplify(max(u.max, v.max)));
}

InfInterval min(InfInterval u, InfInterval v) {
    return InfInterval(simplify(min(u.min, v.min)), simplify(min(u.max, v.max)));
}

InfInterval intersection(InfInterval u, InfInterval v) {
    return InfInterval(simplify(max(u.min, v.min)), simplify(min(u.max, v.max)));
}

InfInterval infinterval_union(InfInterval u, InfInterval v) {
    return InfInterval(simplify(min(u.min, v.min)), simplify(max(u.max, v.max)));
}

InfInterval inverseAdd(InfInterval v, InfInterval k) {
    // Compute an interval such that adding the interval k always results
    // in an interval no bigger than v.
    // (v.min, v.max) = (r.min + k.min, r.max + k.max)
    // r.min = v.min - k.min
    // r.max = v.max - k .max
    return InfInterval(simplify(v.min - k.min), simplify(v.max - k.max));
}

InfInterval inverseSub(InfInterval v, InfInterval k) {
    // Compute an interval such that subtracting the interval k always results
    // in an interval no bigger than v.
    // (v.min, v.max) = (r.min - k.max, r.max - k.min)
    // r.min = v.min + k.max
    // r.max = v.max + k.min
    return InfInterval(simplify(v.min + k.max), simplify(v.max + k.min));
}


namespace Internal {

namespace {
    void check(InfInterval a, std::string op, Expr b, InfInterval result, InfInterval expected) {
        if (! equal(result.min, expected.min) || ! equal(result.max, expected.max)) {
            std::cout << "InfInterval operations test failed: " << a << " " << op << " " << b << "\n";
            std::cout << "  expected: " << expected << "\n" << "  got: " << result << "\n";
            assert(0);
        }
    }
    
    void check(std::string op, InfInterval a, Expr b, InfInterval result, InfInterval expected) {
        if (! equal(result.min, expected.min) || ! equal(result.max, expected.max)) {
            std::cout << "InfInterval operations test failed: " << op << "(" << a << ", " << b << ")\n";
            std::cout << "  expected: " << expected << "\n" << "  got: " << result << "\n";
            assert(0);
        }
    }
    
    void checkzoom(InfInterval a, int div) {
        InfInterval r = zoom(unzoom(a, div), div);
        int absdiv = div > 0 ? div : -div;
        if (r.imin() < a.imin() || r.imax() > a.imax() || r.imin() >= a.imin() + absdiv || r.imax() <= a.imax() - absdiv) {
            std::cout << "InfInterval unzoom and zoom test failed: zoom(unzoom(" << a << ", " << div << "), " << div << ")\n";
            std::cout << "  got: " << r << " from " << unzoom(a, div) << "from" << a << "\n";
            assert(0);
        }
    }
    
    void checkdecimate(InfInterval a, int div) {
        InfInterval r = decimate(a, div) * div;
        int absdiv = div > 0 ? div : -div;
        if (r.imin() < a.imin() || r.imax() > a.imax() || r.imin() >= a.imin() + absdiv || r.imax() <= a.imax() - absdiv) {
            std::cout << "InfInterval decimate and multiply test failed: decimate(" << a << ", " << div << ") * " << div << "\n";
            std::cout << "  got: " << r << " from " << unzoom(a, div) << "\n";
            assert(0);
        }
    }
    
    void checkzoomdiv(InfInterval a, int div) {
        // Check that if you zoom up an interval and then divide it you end
        // up with the original interval.
        // Also check that the new interval has every pixel multiplied div times.
        // Together, these two properties define zoom.
        InfInterval z = zoom(a,div);
        InfInterval r = z / div;
        int lz = z.imax() - z.imin() + 1;
        int la = a.imax() - a.imin() + 1;
        int absdiv = div > 0 ? div : -div;
        if (lz != la * absdiv) {
            std::cout << "InfInterval zoom test failed\n";
            std::cout << "  the original interval contains " << la << " integers\n";
            std::cout << "  the zoomed interval contains " << lz << " integers; expected " << la * absdiv << "\n";
            assert(0);
        }
        check(a, "zoomdiv", div, r, a);
    }
    
    void checkaddinv(InfInterval a, InfInterval b) {
        // Check that if you add two intervals and then apply inverse add
        // you get back the original
        InfInterval z = a + b;
        InfInterval r = inverseAdd(z, b);
        if (! equal(r.min, a.min) || ! equal(r.max, a.max)) {
            std::cout << "InfInterval inverseAdd test failed\n";
            std::cout << "  " << a << " + " << b << " --> " << z << "\n";
            std::cout << "  inverseAdd(" << z << ", " << b << ") --> " << r << "\n";
            assert(0);
        }
    }
    
    void checksubinv(InfInterval a, InfInterval b) {
        // Check that if you subtract two intervals and then apply inverse subtract
        // you get back the original
        InfInterval z = a - b;
        InfInterval r = inverseSub(z, b);
        if (! equal(r.min, a.min) || ! equal(r.max, a.max)) {
            std::cout << "InfInterval inverseSub test failed\n";
            std::cout << "  " << a << " - " << b << " --> " << z << "\n";
            std::cout << "  inverseSub(" << z << ", " << b << ") --> " << r << "\n";
            assert(0);
        }
    }
}

void infinterval_test () {
    InfInterval v1(5,1282);
    InfInterval v2(6,1281);
    InfInterval v3(7,1280); // Just outside the boundaries
    InfInterval v4(8,1279); // Exactly on the boundaries of multiples of 8
    InfInterval v5(9,1278); // Just inside the boundaries
    InfInterval v6(10,1277);
    InfInterval v7(11,1276);
    InfInterval va(1,159);
    InfInterval vb(-159,-1);
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
    std::cout << "InfInterval operations test passed\n";
}
}

// end namespace Halide
}

