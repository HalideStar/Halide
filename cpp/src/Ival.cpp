#include "IR.h"
#include "IROperator.h"
#include "Simplify.h"
#include "IRPrinter.h"
#include "IREquality.h"

namespace Halide {

/** Constructors that use infinity */
Ival::Ival() : min(new Internal::Infinity(-1)), max(new Internal::Infinity(1)) {}
    
Ival::Ival(Interval i) : min(i.min.defined() ? i.min : new Internal::Infinity(-1)),
                       max(i.max.defined() ? i.max : new Internal::Infinity(1)) {}

/** Implementation of manipulation of Ivals. */
Ival operator+(Ival v, Expr b) { 
    return Ival(simplify(v.min + b), simplify(v.max + b)); 
}

Ival operator-(Ival v, Expr b) { 
    return Ival(simplify(v.min - b), simplify(v.max - b)); 
}

Ival operator-(Ival v) { 
    return Ival(simplify(-v.max), simplify(-v.min)); 
}

Ival operator*(Ival v, Expr b) { 
    return Ival(simplify(select(b >= 0, v.min * b, v.max * b)), simplify(select(b >= 0, v.max * b, v.min * b))); 
}

Ival operator/(Ival v, Expr b) { 
    return Ival(simplify(select(b >= 0, v.min / b, v.max / b)), simplify(select(b >= 0, v.max / b, v.min / b))); 
}

Ival zoom(Ival v, Expr b) { 
    // Multiplying an Ival (by an integer other than zero).
    // For positive b, the new Ival is min*b, max*b+(b-1).
    // For negative b, the new Ival is max*b, min*b-(b+1).
    // This is symmetric with unzoom
    // Other definitions could be used - this definition always appends the
    // additional space to the top end of the Ival.
    
    if (b.type().is_float() || v.min.type().is_float() || v.max.type().is_float()) {
        // Result will be floating type, not integer.  zoom degenerates to multiply.
        return v * b;
    }

    Expr newmin = select(b >= 0, v.min * b, v.max * b);
    Expr newmax = select(b >= 0, v.max * b + (b-1), v.min * b - (b+1));
    return Ival(simplify(newmin), simplify(newmax)); 
}

Ival decimate(Ival v, Expr b) { 
    // Decimate an Ival.  Get every element of v
    // that is a multiple of b, and then divide them by b.
    
    if (b.type().is_float() || v.min.type().is_float() || v.max.type().is_float()) {
        // Result will be floating type, not integer.  decimate degenerates to division.
        return v / b;
    }

    // (x - 1) / b + 1 is ceiling for positive b.
    // (x + 1) / b + 1 is ceiling for negative b.
    Expr newmin = select(b >= 0, (v.min - 1) / b + 1, (v.max + 1) / b + 1);
    Expr newmax = select(b >= 0, v.max / b, (v.min) / b);
    return Ival(simplify(newmin), simplify(newmax));
}

Ival unzoom(Ival v, Expr b) { 
    // Unzoom an Ival.  The semantics is that the
    // result should be such that if you zoom the Ival
    // up again then it will not exceed the original Ival.
    
    // Halide division is floor.  To get ceiling for a positive divisor,
    // compute (x - 1) / b + 1.  For a negative divisor: (x + 1) / b + 1.
    
    if (b.type().is_float() || v.min.type().is_float() || v.max.type().is_float()) {
        // Result will be floating type, not integer.  unzoom degenerates to division.
        return v / b;
    }

    Expr newmin = select(b >= 0, (v.min - 1) / b + 1, (v.max + 2) / b + 2);
    Expr newmax = select(b >= 0, (v.max + 1) / b - 1, (v.min) / b);
    return Ival(simplify(newmin), simplify(newmax));
}

# if 0
Ival operator%(Ival v, Expr b) { 
    return Ival(simplify(v.min % b), simplify(v.max % b)); 
}
# endif

int Ival::imin() {
    int ival;
    assert (get_const_int(min, ival) && "Expected an integer value in the Ival");
    return ival;
}

int Ival::imax() {
    int ival;
    assert (get_const_int(max, ival) && "Expected an integer value in the Ival");
    return ival;
}


// ---------------------------------------
// Operators on pairs of Ivals.

Ival operator+(Ival u, Ival v) {
    return Ival(simplify(u.min + v.min), simplify(u.max + v.max));
}

Ival operator-(Ival u, Ival v) {
    return Ival(simplify(u.min - v.max), simplify(u.max - v.min));
}

Ival operator*(Ival u, Ival v) {
    // Special-case optimizations to generate less work for the constant-folder
    if (is_const(u.min) && equal(u.min, u.max)) {
        if (is_negative_const(u.min)) std::swap(v.min, v.max);
        return Ival(simplify(v.min * u.min), simplify(v.max * u.min));
    } else if (is_const(v.min) && equal(v.min, v.max)) {
        if (is_negative_const(v.min)) std::swap(u.min, u.max);
        return Ival(simplify(u.min * v.min), simplify(u.max * v.min));           
    } else {
        Expr a = u.min * v.min;
        Expr b = u.min * v.max;
        Expr c = u.max * v.min;
        Expr d = u.max * v.max;
        
        Expr rmin = min(min(a, b), min(c, d));
        Expr rmax = max(max(a, b), max(c, d));
        return Ival(simplify(rmin), simplify(rmax));
    }
}

Ival operator/(Ival u, Ival v) {
    if (is_const(v.min) && equal(v.min, v.max)) {
        if (is_negative_const(v.min)) std::swap(u.min, u.max); 
        return Ival(simplify(u.min / v.min), simplify(u.max / v.min));
    } else {
        // if we can't statically prove that the divisor can't span zero, then we're unbounded
        bool min_v_is_positive = proved(v.min > Internal::make_zero(v.min.type()));
        bool max_v_is_negative = proved(v.max < Internal::make_zero(v.max.type()));
        if (! min_v_is_positive && ! max_v_is_negative) {
            return Ival(Expr(new Internal::Infinity(-1)), Expr(new Internal::Infinity(1)));
        } else {
            Expr a = u.min / v.min;
            Expr b = u.min / v.max;
            Expr c = u.max / v.min;
            Expr d = u.max / v.max;
            
            Expr rmin = min(min(a, b), min(c, d));
            Expr rmax = max(max(a, b), max(c, d));
            return Ival(simplify(rmin), simplify(rmax));
        }
    }
}

Ival operator%(Ival u, Ival v) {
    // If we can prove that u is in the range of mod v then Ival u is the result. 
    if (proved(u.min >= 0) && proved(u.max < v.min)) {
        // u is a positive Ival and so is v.
        return u;
    } else if (proved(u.max <= 0) && proved(u.min > v.max)) {
        // u is a negative Ival and so is v.
        return u;
    } else {
        Expr rmin = Internal::make_zero(u.min.type());
        Expr rmax;
        if (v.max.type().is_float()) {
            rmax = v.max;
        } else {
            rmax = v.max - 1;
        }
        return Ival(simplify(rmin), simplify(rmax));
    }
}

Ival max(Ival u, Ival v) {
    return Ival(simplify(max(u.min, v.min)), simplify(max(u.max, v.max)));
}

Ival min(Ival u, Ival v) {
    return Ival(simplify(min(u.min, v.min)), simplify(min(u.max, v.max)));
}

Ival intersection(Ival u, Ival v) {
    return Ival(simplify(max(u.min, v.min)), simplify(min(u.max, v.max)));
}

Ival ival_union(Ival u, Ival v) {
    return Ival(simplify(min(u.min, v.min)), simplify(max(u.max, v.max)));
}

namespace Internal {

namespace {
    void check(Ival a, std::string op, Expr b, Ival result, Ival expected) {
        if (! equal(result.min, expected.min) || ! equal(result.max, expected.max)) {
            std::cout << "Ival operations test failed: " << a << " " << op << " " << b << "\n";
            std::cout << "  expected: " << expected << "\n" << "  got: " << result << "\n";
            assert(0);
        }
    }
    
    void check(std::string op, Ival a, Expr b, Ival result, Ival expected) {
        if (! equal(result.min, expected.min) || ! equal(result.max, expected.max)) {
            std::cout << "Ival operations test failed: " << op << "(" << a << ", " << b << ")\n";
            std::cout << "  expected: " << expected << "\n" << "  got: " << result << "\n";
            assert(0);
        }
    }
    
    void checkzoom(Ival a, int div) {
        Ival r = zoom(unzoom(a, div), div);
        int absdiv = div > 0 ? div : -div;
        if (r.imin() < a.imin() || r.imax() > a.imax() || r.imin() >= a.imin() + absdiv || r.imax() <= a.imax() - absdiv) {
            std::cout << "Ival unzoom and zoom test failed: zoom(unzoom(" << a << ", " << div << "), " << div << ")\n";
            std::cout << "  got: " << r << " from " << unzoom(a, div) << "\n";
        }
    }
    
    void checkdecimate(Ival a, int div) {
        Ival r = decimate(a, div) * div;
        int absdiv = div > 0 ? div : -div;
        if (r.imin() < a.imin() || r.imax() > a.imax() || r.imin() >= a.imin() + absdiv || r.imax() <= a.imax() - absdiv) {
            std::cout << "Ival decimate and multiply test failed: decimate(" << a << ", " << div << ") * " << div << "\n";
            std::cout << "  got: " << r << " from " << unzoom(a, div) << "\n";
        }
    }
}

void Ival_test () {
    Ival v1(5,1282);
    Ival v2(6,1281);
    Ival v3(7,1280); // Just outside the boundaries
    Ival v4(8,1279); // Exactly on the boundaries of multiples of 8
    Ival v5(9,1278); // Just inside the boundaries
    Ival v6(10,1277);
    Ival v7(11,1276);
    Ival va(1,159);
    Ival vb(-159,-1);
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
    check("zoom", va, Expr(8), zoom(va, 8), Ival(8, 1279));
    check("zoom", va, Expr(8), zoom(vb, 8), Ival(-1272, -1));
    check("zoom", va, Expr(8), zoom(va, -8), Ival(-1272, -1));
    check("zoom", va, Expr(8), zoom(vb, -8), Ival(8, 1279));
    std::cout << "Ival operations test passed";
}
}

// end namespace Halide
}

