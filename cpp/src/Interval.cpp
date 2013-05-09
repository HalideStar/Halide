#include "IR.h"
#include "IROperator.h"
#include "Simplify.h"
#include "IRPrinter.h"
#include "IREquality.h"

namespace Halide {

/** Implementation of manipulation of intervals. */

Interval operator*(Interval v, Expr b) { 
    bool def_min = v.min.defined(), def_max = v.max.defined();
    Expr rmin, rmax;
    
    if (proved(b >= 0)) {
        if (def_min) rmin = v.min * b;
        if (def_max) rmax = v.max * b;
    } else if (proved(b <= 0)) {
        if (def_max) rmin = v.max * b;
        if (def_min) rmax = v.min * b;
    } else if (def_min && def_max) {
        rmin = select(b >= 0, v.min * b, v.max * b);
        rmax = select(b >= 0, v.max * b, v.min * b);
    }
    return Interval(simplify(rmin), simplify(rmax));
}

Interval operator/(Interval v, Expr b) { 
    bool def_min = v.min.defined(), def_max = v.max.defined();
    Expr rmin, rmax;
    
    b = simplify(b);
    
    if (is_zero(b)) {
        return Interval(rmin, rmax); // Division by zero is unbounded.
    } else if (proved(b > 0)) {
        if (def_min) rmin = v.min / b;
        if (def_max) rmax = v.max / b;
    } else if (proved(b < 0)) {
        if (def_max) rmin = v.max / b;
        if (def_min) rmax = v.min / b;
    } else if (def_min && def_max) {
        rmin = select(b > 0, v.min / b, v.max / b);
        rmax = select(b > 0, v.max / b, v.min / b);
    }
    return Interval(simplify(rmin), simplify(rmax));
}

# if 0
Interval operator+(Interval v, Expr b) { 
    return Interval(simplify(v.min + b), simplify(v.max + b)); 
}

Interval operator-(Interval v, Expr b) { 
    return Interval(simplify(v.min - b), simplify(v.max - b)); 
}

Interval operator-(Interval v) { 
    return Interval(simplify(-v.max), simplify(-v.min)); 
}


Interval operator%(Interval v, Expr b) { 
    return Interval(simplify(v.min % b), simplify(v.max % b)); 
}
# endif

Interval zoom(Interval v, Expr b) { 
    // Multiplying an interval (by an integer other than zero).
    // For positive b, the new interval is min*b, max*b+(b-1).
    // For negative b, the new interval is max*b, min*b-(b+1).
    // This is symmetric with unzoom
    // Other definitions could be used - this definition always appends the
    // additional space to the top end of the interval.
    
    bool def_min = v.min.defined(), def_max = v.max.defined();
    Expr rmin, rmax;
    
    if (b.type().is_float() || v.min.type().is_float() || v.max.type().is_float()) {
        // Result will be floating type, not integer.  zoom degenerates to multiply.
        return v * b;
    }

    if (proved(b >= 0)) {
        if (def_min) rmin = v.min * b;
        if (def_max) rmax = v.max * b + (b - 1);
    } else if (proved(b <= 0)) {
        if (def_max) rmin = v.max * b;
        if (def_min) rmax = v.min * b - (b + 1);
    } else if (def_min && def_max) {
        rmin = select(b >= 0, v.min * b, v.max * b);
        rmax = select(b >= 0, v.max * b + (b-1), v.min * b - (b+1));
    }
    return Interval(simplify(rmin), simplify(rmax));
}

Interval decimate(Interval v, Expr b) { 
    // Decimate an interval.  Get every element of v
    // which is a multiple of b, and then divide them by b.
    
    bool def_min = v.min.defined(), def_max = v.max.defined();
    Expr rmin, rmax;
    
    if (b.type().is_float() || v.min.type().is_float() || v.max.type().is_float()) {
        // Result will be floating type, not integer.  decimate degenerates to division.
        return v / b;
    }

    // (x - 1) / b + 1 is ceiling for positive b.
    // (x + 1) / b + 1 is ceiling for negative b.
    if (proved(b >= 0)) {
        if (def_min) rmin = (v.min - 1) / b + 1;
        if (def_max) rmax = v.max / b;
    } else if (proved(b <= 0)) {
        if (def_max) rmin = (v.max + 1) / b + 1;
        if (def_min) rmax = v.min / b;
    } else if (def_min && def_max) {
        rmin = select(b >= 0, (v.min - 1) / b + 1, (v.max + 1) / b + 1);
        rmax = select(b >= 0, v.max / b, v.min / b);
    }
    return Interval(simplify(rmin), simplify(rmax));
}

Interval unzoom(Interval v, Expr b) { 
    // Unzoom an interval.  The semantics is that the
    // result should be such that if you zoom the interval
    // up again then it will not exceed the original interval.
    
    bool def_min = v.min.defined(), def_max = v.max.defined();
    Expr rmin, rmax;
    
    if (b.type().is_float() || v.min.type().is_float() || v.max.type().is_float()) {
        // Result will be floating type, not integer.  unzoom degenerates to division.
        return v / b;
    }
    
    if (proved(b >= 0)) {
        if (def_min) rmin = (v.min - 1) / b + 1;
        if (def_max) rmax = (v.max + 1) / b - 1;
    } else if (proved(b <= 0)) {
        if (def_max) rmin = (v.max + 2) / b + 2;
        if (def_min) rmax = v.min / b;
    } else if (def_min && def_max) {
        rmin = select(b >= 0, (v.min - 1) / b + 1, (v.max + 2) / b + 2);
        rmax = select(b >= 0, (v.max + 1) / b - 1, (v.min) / b);
    }
    return Interval(simplify(rmin), simplify(rmax));
}

int Interval::imin() {
    int ival;
    assert (get_const_int(min, ival) && "Expected an integer value in the interval");
    return ival;
}

int Interval::imax() {
    int ival;
    assert (get_const_int(max, ival) && "Expected an integer value in the interval");
    return ival;
}


# if 0
// ---------------------------------------
// Operators on pairs of intervals.

Interval operator+(Interval u, Interval v) {
    return Interval(simplify(u.min + v.min), simplify(u.max + v.max));
}

Interval operator-(Interval u, Interval v) {
    return Interval(simplify(u.min - v.max), simplify(u.max - v.min));
}

Interval operator*(Interval u, Interval v) {
    // Special-case optimizations to generate less work for the constant-folder
    if (is_const(u.min) && equal(u.min, u.max)) {
        if (is_negative_const(u.min)) std::swap(v.min, v.max);
        return Interval(simplify(v.min * u.min), simplify(v.max * u.min));
    } else if (is_const(v.min) && equal(v.min, v.max)) {
        if (is_negative_const(v.min)) std::swap(u.min, u.max);
        return Interval(simplify(u.min * v.min), simplify(u.max * v.min));           
    } else {
        Expr a = u.min * v.min;
        Expr b = u.min * v.max;
        Expr c = u.max * v.min;
        Expr d = u.max * v.max;
        
        Expr rmin = min(min(a, b), min(c, d));
        Expr rmax = max(max(a, b), max(c, d));
        return Interval(simplify(rmin), simplify(rmax));
    }
}

Interval operator/(Interval u, Interval v) {
    if (is_const(v.min) && equal(v.min, v.max)) {
        if (is_negative_const(v.min)) std::swap(u.min, u.max); 
        return Interval(simplify(u.min / v.min), simplify(u.max / v.min));
    } else {
        // if we can't statically prove that the divisor can't span zero, then we're unbounded
        bool min_v_is_positive = proved(v.min > Internal::make_zero(v.min.type()));
        bool max_v_is_negative = proved(v.max < Internal::make_zero(v.max.type()));
        if (! min_v_is_positive && ! max_v_is_negative) {
            return Interval(Expr(new Internal::Infinity(-1)), Expr(new Internal::Infinity(1)));
        } else {
            Expr a = u.min / v.min;
            Expr b = u.min / v.max;
            Expr c = u.max / v.min;
            Expr d = u.max / v.max;
            
            Expr rmin = min(min(a, b), min(c, d));
            Expr rmax = max(max(a, b), max(c, d));
            return Interval(simplify(rmin), simplify(rmax));
        }
    }
}

Interval operator%(Interval u, Interval v) {
    // If we can prove that u is in the range of mod v then interval u is the result. 
    if (proved(u.min >= 0) && proved(u.max < v.min)) {
        // u is a positive interval and so is v.
        return u;
    } else if (proved(u.max <= 0) && proved(u.min > v.max)) {
        // u is a negative interval and so is v.
        return u;
    } else {
        Expr rmin = Internal::make_zero(u.min.type());
        Expr rmax;
        if (v.max.type().is_float()) {
            rmax = v.max;
        } else {
            rmax = v.max - 1;
        }
        return Interval(simplify(rmin), simplify(rmax));
    }
}

Interval max(Interval u, Interval v) {
    return Interval(simplify(max(u.min, v.min)), simplify(max(u.max, v.max)));
}

Interval min(Interval u, Interval v) {
    return Interval(simplify(min(u.min, v.min)), simplify(min(u.max, v.max)));
}
# endif

Interval intersection(Interval u, Interval v) {
    Expr rmin, rmax;
    if (! u.min.defined()) rmin = v.min;
    else if (! v.min.defined()) rmin = u.min;
    else rmin = simplify(max(u.min, v.min));
    if (! u.max.defined()) rmax = v.max;
    else if (! v.max.defined()) rmax = u.max;
    else rmax = simplify(min(u.max, v.max));
    return Interval(rmin, rmax);
}

Interval interval_union(Interval u, Interval v) {
    Expr rmin, rmax;
    if (u.min.defined() && v.min.defined()) rmin = simplify(min(u.min, v.min));
    if (u.max.defined() && v.max.defined()) rmax = simplify(max(u.max, v.max));
    return Interval(rmin, rmax);
}

namespace Internal {

namespace {
    void check(Interval a, std::string op, Expr b, Interval result, Interval expected) {
        if (! equal(result.min, expected.min) || ! equal(result.max, expected.max)) {
            std::cout << "Interval operations test failed: " << a << " " << op << " " << b << "\n";
            std::cout << "  expected: " << expected << "\n" << "  got: " << result << "\n";
            assert(0);
        }
    }
    
    void check(std::string op, Interval a, Expr b, Interval result, Interval expected) {
        if (! equal(result.min, expected.min) || ! equal(result.max, expected.max)) {
            std::cout << "Interval operations test failed: " << op << "(" << a << ", " << b << ")\n";
            std::cout << "  expected: " << expected << "\n" << "  got: " << result << "\n";
            assert(0);
        }
    }
    
    void checkzoom(Interval a, int div) {
        Interval r = zoom(unzoom(a, div), div);
        int absdiv = div > 0 ? div : -div;
        if (r.imin() < a.imin() || r.imax() > a.imax() || r.imin() >= a.imin() + absdiv || r.imax() <= a.imax() - absdiv) {
            std::cout << "Interval unzoom and zoom test failed: zoom(unzoom(" << a << ", " << div << "), " << div << ")\n";
            std::cout << "  got: " << r << " from " << unzoom(a, div) << "\n";
        }
    }
    
    void checkdecimate(Interval a, int div) {
        Interval r = decimate(a, div) * div;
        int absdiv = div > 0 ? div : -div;
        if (r.imin() < a.imin() || r.imax() > a.imax() || r.imin() >= a.imin() + absdiv || r.imax() <= a.imax() - absdiv) {
            std::cout << "Interval decimate and multiply test failed: decimate(" << a << ", " << div << ") * " << div << "\n";
            std::cout << "  got: " << r << " from " << unzoom(a, div) << "\n";
        }
    }
}

void interval_test () {
    Interval v1(5,1282);
    Interval v2(6,1281);
    Interval v3(7,1280); // Just outside the boundaries
    Interval v4(8,1279); // Exactly on the boundaries of multiples of 8
    Interval v5(9,1278); // Just inside the boundaries
    Interval v6(10,1277);
    Interval v7(11,1276);
    Interval va(1,159);
    Interval vb(-159,-1);
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
    check("zoom", va, Expr(8), zoom(va, 8), Interval(8, 1279));
    check("zoom", va, Expr(8), zoom(vb, 8), Interval(-1272, -1));
    check("zoom", va, Expr(8), zoom(va, -8), Interval(-1272, -1));
    check("zoom", va, Expr(8), zoom(vb, -8), Interval(8, 1279));
    std::cout << "Interval operations test passed\n";
}
}

// end namespace Halide
}

