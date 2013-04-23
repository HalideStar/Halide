#include "IR.h"
#include "IROperator.h"
#include "Simplify.h"
#include "IRPrinter.h"
#include "IREquality.h"

namespace Halide {

/** Implementation of manipulation of intervals. */
Interval operator+(Interval v, Expr b) { 
    return Interval(simplify(v.min + b), simplify(v.max + b)); 
}

Interval operator-(Interval v, Expr b) { 
    return Interval(simplify(v.min - b), simplify(v.max - b)); 
}

Interval operator-(Interval v) { 
    return Interval(simplify(-v.max), simplify(-v.min)); 
}

Interval operator*(Interval v, Expr b) { 
    return Interval(simplify(select(b >= 0, v.min * b, v.max * b)), simplify(select(b >= 0, v.max * b, v.min * b))); 
}

Interval operator/(Interval v, Expr b) { 
    return Interval(simplify(select(b >= 0, v.min / b, v.max / b)), simplify(select(b >= 0, v.max / b, v.min / b))); 
}

Interval zoom(Interval v, Expr b) { 
    // Multiplying an interval (by an integer other than zero).
    // For positive b, the new interval is min*b, max*b+(b-1).
    // For negative b, the new interval is max*b, min*b-(b+1).
    // This is symmetric with unzoom
    // Other definitions could be used - this definition always appends the
    // additional space to the top end of the interval.
    
    if (! v.min.defined() && ! v.max.defined()) return v; // Undefined result.
    
    // Convert an undefined end of the interval to infinity.
    if (! v.min.defined()) v.min = new Internal::Infinity(v.max.type(), -1);
    else if (! v.max.defined()) v.max = new Internal::Infinity(v.min.type(), 1);
    
    if (b.type().is_float() || v.min.type().is_float() || v.max.type().is_float()) {
        // Result will be floating type, not integer.  zoom degenerates to multiply.
        return Interval(simplify(v.min * b), simplify(v.max * b));
    }

    Expr newmin = select(b >= 0, v.min * b, v.max * b);
    Expr newmax = select(b >= 0, v.max * b + (b-1), v.min * b - (b+1));
    return Interval(simplify(newmin), simplify(newmax)); 
}

Interval decimate(Interval v, Expr b) { 
    // Decimate an interval.  Get every element of v
    // that is a multiple of b, and then divide them by b.
    
    if (! v.min.defined() && ! v.max.defined()) return v; // Undefined result.
    
    // Convert an undefined end of the interval to infinity.
    if (! v.min.defined()) v.min = new Internal::Infinity(v.max.type(), -1);
    else if (! v.max.defined()) v.max = new Internal::Infinity(v.min.type(), 1);
    
    if (b.type().is_float() || v.min.type().is_float() || v.max.type().is_float()) {
        // Result will be floating type, not integer.  decimate degenerates to division.
        return Interval(simplify(v.min / b), simplify(v.max / b));
    }

    // (x - 1) / b + 1 is ceiling for positive b.
    // (x + 1) / b + 1 is ceiling for negative b.
    Expr newmin = select(b >= 0, (v.min - 1) / b + 1, (v.max + 1) / b + 1);
    Expr newmax = select(b >= 0, v.max / b, (v.min) / b);
    return Interval(simplify(newmin), simplify(newmax));
}

Interval unzoom(Interval v, Expr b) { 
    // Unzoom an interval.  The semantics is that the
    // result should be such that if you zoom the interval
    // up again then it will not exceed the original interval.
    
    // Halide division is floor.  To get ceiling for a positive divisor,
    // compute (x - 1) / b + 1.  For a negative divisor: (x + 1) / b + 1.
    
    if (! v.min.defined() && ! v.max.defined()) return v; // Undefined result.
    
    // Convert an undefined end of the interval to infinity.
    if (! v.min.defined()) v.min = new Internal::Infinity(v.max.type(), -1);
    else if (! v.max.defined()) v.max = new Internal::Infinity(v.min.type(), 1);

    if (b.type().is_float() || v.min.type().is_float() || v.max.type().is_float()) {
        // Result will be floating type, not integer.  unzoom degenerates to division.
        return Interval(simplify(v.min / b), simplify(v.max / b));
    }

    Expr newmin = select(b >= 0, (v.min - 1) / b + 1, (v.max + 2) / b + 2);
    Expr newmax = select(b >= 0, (v.max + 1) / b - 1, (v.min) / b);
    return Interval(simplify(newmin), simplify(newmax));
}

Interval operator%(Interval v, Expr b) { 
    return Interval(simplify(v.min % b), simplify(v.max % b)); 
}

Interval intersection(Interval u, Interval v) {
    return Interval(simplify(max(u.min, v.min)), simplify(min(u.max, v.max)));
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
    std::cout << "Interval operations test passed";
}
}

// end namespace Halide
}

