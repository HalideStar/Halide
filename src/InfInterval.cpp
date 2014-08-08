#include "IR.h"
#include "IROperator.h"
#include "Simplify.h"
#include "IRPrinter.h"
#include "IREquality.h"

namespace Halide {

/** Implementation of manipulation of InfIntervals. */
InfInterval operator+(InfInterval v, Expr b) { 
    return InfInterval(simplify(v.min + b), simplify(v.max + b)); 
}

InfInterval operator-(InfInterval v, Expr b) { 
    return InfInterval(simplify(v.min - b), simplify(v.max - b)); 
}

InfInterval operator-(InfInterval v) { 
    return InfInterval(simplify(-v.max), simplify(-v.min)); 
}

InfInterval operator*(InfInterval v, Expr b) { 
    return InfInterval(simplify(select(b >= 0, v.min * b, v.max * b)), simplify(select(b >= 0, v.max * b, v.min * b))); 
}

InfInterval operator/(InfInterval v, Expr b) { 
    return InfInterval(simplify(select(b >= 0, v.min / b, v.max / b)), simplify(select(b >= 0, v.max / b, v.min / b))); 
}

InfInterval zoom(InfInterval v, Expr b) { 
    // Multiplying an InfInterval (by an integer other than zero).
    // For positive b, the new InfInterval is min*b, max*b+(b-1).
    // For negative b, the new InfInterval is max*b, min*b-(b+1).
    // This is symmetric with unzoom
    // Other definitions could be used - this definition always appends the
    // additional space to the top end of the InfInterval.
    
    if (b.type().is_float() || v.min.type().is_float() || v.max.type().is_float()) {
        // Result will be floating type, not integer.  zoom degenerates to multiply.
        return v * b;
    }

    Expr newmin = select(b >= 0, v.min * b, v.max * b);
    Expr newmax = select(b >= 0, v.max * b + (b-1), v.min * b - (b+1));
    return InfInterval(simplify(newmin), simplify(newmax)); 
}

InfInterval decimate(InfInterval v, Expr b) { 
    // Decimate an InfInterval.  Get every element of v
    // that is a multiple of b, and then divide them by b.
    
    if (b.type().is_float() || v.min.type().is_float() || v.max.type().is_float()) {
        // Result will be floating type, not integer.  decimate degenerates to division.
        return v / b;
    }

    // (x - 1) / b + 1 is ceiling for positive b.
    // (x + 1) / b + 1 is ceiling for negative b.
    Expr newmin = select(b >= 0, (v.min - 1) / b + 1, (v.max + 1) / b + 1);
    Expr newmax = select(b >= 0, v.max / b, (v.min) / b);
    return InfInterval(simplify(newmin), simplify(newmax));
}

InfInterval unzoom(InfInterval v, Expr b) { 
    // Unzoom an InfInterval.  The semantics is that the
    // result should be such that if you zoom the InfInterval
    // up again then it will not exceed the original InfInterval.
    
    // Halide division is floor.  To get ceiling for a positive divisor,
    // compute (x - 1) / b + 1.  For a negative divisor: (x + 1) / b + 1.
    
    if (b.type().is_float() || v.min.type().is_float() || v.max.type().is_float()) {
        // Result will be floating type, not integer.  unzoom degenerates to division.
        return v / b;
    }

    Expr newmin = select(b >= 0, (v.min - 1) / b + 1, (v.max + 2) / b + 2);
    Expr newmax = select(b >= 0, (v.max + 1) / b - 1, (v.min) / b);
    return InfInterval(simplify(newmin), simplify(newmax));
}

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
            return InfInterval(Internal::Infinity::make(-1), Internal::Infinity::make(1));
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

InfInterval ival_union(InfInterval u, InfInterval v) {
    return InfInterval(simplify(min(u.min, v.min)), simplify(max(u.max, v.max)));
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
            std::cout << "  got: " << r << " from " << unzoom(a, div) << "\n";
        }
    }
    
    void checkdecimate(InfInterval a, int div) {
        InfInterval r = decimate(a, div) * div;
        int absdiv = div > 0 ? div : -div;
        if (r.imin() < a.imin() || r.imax() > a.imax() || r.imin() >= a.imin() + absdiv || r.imax() <= a.imax() - absdiv) {
            std::cout << "InfInterval decimate and multiply test failed: decimate(" << a << ", " << div << ") * " << div << "\n";
            std::cout << "  got: " << r << " from " << unzoom(a, div) << "\n";
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
    check("zoom", va, Expr(8), zoom(va, 8), InfInterval(8, 1279));
    check("zoom", va, Expr(8), zoom(vb, 8), InfInterval(-1272, -1));
    check("zoom", va, Expr(8), zoom(va, -8), InfInterval(-1272, -1));
    check("zoom", va, Expr(8), zoom(vb, -8), InfInterval(8, 1279));
    std::cout << "InfInterval operations test passed\n";
}
}

// end namespace Halide
}

