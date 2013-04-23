#include "IR.h"
#include "IROperator.h"
#include "Simplify.h"

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
    // Multiplying an interval (by an integer other than zero).
    // For positive b, the new interval is min*b, max*b+(b-1).
    // For negative b, the new interval is max*b, min*b-(b+1).
    // This is symmetric with operator/
    // Other definitions could be used - this definition always appends the
    // additional space to the top end of the interval.
    Expr newmin = select(b >= 0, v.min * b, v.max * b);
    Expr newmax = select(b >= 0, v.max * b + (b-1), v.min * b - (b+1));
    return Interval(simplify(newmin), simplify(newmax)); 
}

Interval operator/(Interval v, Expr b) { 
    // Dividing an interval.  The semantics is that the
    // result of the division should be such that if you multiply the interval
    // up again then it will not exceed the original interval.
    // For positive divisors, we have min <- ceil(min/b) and max <- floor(max/b).
    // For negative divisors, we have min <- ceil(max/b) and max <- floor(min/b).
    // When the divisor is positive, floor(min/b) is less than floor(max/b).
    // When the divisor is negative, floor(max/b) is less than floor(min/b).
    // So, take maximum of the two floors and minimum of the two ceils.

    // Note: Halide defined division as floor.
    Expr newmax = select(b >= 0, v.max / b, v.min / b);
    // To compute ceiling, if the divisor is positive then subtract one
    // from the denominator, else add one to the denominator, do the division
    // and finally add one to the result.
    Expr newmin = select(b >= 0, (v.min - 1) / b + 1, (v.max + 1) / b + 1);
    return Interval(simplify(min(newmin, newmax)), simplify(max(newmin, newmax)));
}

Interval operator%(Interval v, Expr b) { 
    return Interval(simplify(v.min % b), simplify(v.max % b)); 
}

Interval intersection(Interval u, Interval v) {
    return Interval(simplify(max(u.min, v.min)), simplify(min(u.max, v.max)));
}
// end namespace Halide
}

