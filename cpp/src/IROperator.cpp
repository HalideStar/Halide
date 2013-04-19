#include "IROperator.h"
#include "IRPrinter.h"
#include "Simplify.h"
#include <iostream>

namespace Halide { 
namespace Internal {

bool is_const(Expr e) {
    if (e.as<IntImm>()) return true;
    if (e.as<FloatImm>()) return true;
    if (const Cast *c = e.as<Cast>()) {
        return is_const(c->value);
    }
    if (const Ramp *r = e.as<Ramp>()) {
        return is_const(r->base) && is_const(r->stride);
    }

    if (const Broadcast *b = e.as<Broadcast>()) {
        return is_const(b->value);
    }
    return false;
            
}

bool is_const(Expr e, int value) {
    if (const IntImm *i = e.as<IntImm>()) return i->value == value;
    if (const FloatImm *i = e.as<FloatImm>()) return i->value == value;
    if (const Cast *c = e.as<Cast>()) return is_const(c->value, value);
    if (const Broadcast *b = e.as<Broadcast>()) return is_const(b->value, value);
    return false;
}

namespace {
bool get_const_int_rec(Expr e, int &value) {
    if (! e.defined()) return false; // Undefined!
    // Integer constant.  Ensure that the returned value captures all the data.
    // (This code design ensures correct operation irrespective of the size
    // of i->value).
    if (const IntImm *i = e.as<IntImm>()) { 
        int ival = i->value; 
        if (ival == i->value) {
            value = ival;
            return true;
        }
        return false; 
    }
    // Floating point immediate is only accepted if the value is integral,
    // otherwise there would be loss of data.
    if (const FloatImm *i = e.as<FloatImm>()) { 
        int ival = i->value; 
        if (ival == i->value) {
            value = ival;
            return true;
        }
        return false;
    }
    if (const Cast *c = e.as<Cast>()) {
        int ival;
        bool status = get_const_int_rec(c->value, ival);
        if (status) {
            // Check on the type of the cast.
            // Checks on the underlying value ensure that ival accurately
            // reflects the underlying constant value.  Now we force it into
            // range according to the cast and ensure that the result is
            // correctly interpreted as an int.
            if (c->type.is_int() || c->type.is_uint()) {
                // Cast to an integer type.  Force the value into range.
                int cval = int_cast_constant(c->type, ival);
                // Check that the cast is not returning negative from unsigned.
                // This will only happen when c->type.bits == sizeof(int) * 8
                // and then only when the unsigned integer constant is very large.
                if (c->type.is_uint() && cval < 0) return false;
                value = cval;
                return true;
            } else if (c->type.is_float()) {
                // Cast to a float type. Just accept the value from underneath.
                value = ival;
                return status;
            }
            return false; // Could not handle the cast
        }
        return false;
    }
    if (const Broadcast *b = e.as<Broadcast>()) return get_const_int_rec(b->value, value);
    return false;
}
}

bool get_const_int(Expr e, int &value) {
    bool status;
    status = get_const_int_rec(e, value);
    if (! status) e = simplify(e);
    status = get_const_int_rec(e, value);
    return status;
}

bool is_const_power_of_two(Expr e, int *bits) {
    const Broadcast *b = e.as<Broadcast>();
    if (b) return is_const_power_of_two(b->value, bits);
    
    const Cast *c = e.as<Cast>();
    if (c) return is_const_power_of_two(c->value, bits);

    const IntImm *int_imm = e.as<IntImm>();
    if (int_imm) {
        int bit_count = 0;
        int tmp;
        for (tmp = 1; tmp < int_imm->value; tmp *= 2) {
            bit_count++;
        }
        if (tmp == int_imm->value) {
            *bits = bit_count;
            return true;
        }
    }
    return false;
}

bool is_positive_const(Expr e) {
    if (const IntImm *i = e.as<IntImm>()) return i->value > 0;
    if (const FloatImm *f = e.as<FloatImm>()) return f->value > 0.0f;
    if (const Cast *c = e.as<Cast>()) {
        return is_positive_const(c->value);
    }
    if (const Ramp *r = e.as<Ramp>()) {
        // slightly conservative
        return is_positive_const(r->base) && is_positive_const(r->stride);
    }
    if (const Broadcast *b = e.as<Broadcast>()) {
        return is_positive_const(b->value);
    }
    return false;
}

bool is_negative_const(Expr e) {
    if (const IntImm *i = e.as<IntImm>()) return i->value < 0;
    if (const FloatImm *f = e.as<FloatImm>()) return f->value < 0.0f;
    if (const Cast *c = e.as<Cast>()) {
        return is_negative_const(c->value);
    }
    if (const Ramp *r = e.as<Ramp>()) {
        // slightly conservative
        return is_negative_const(r->base) && is_negative_const(r->stride);
    }
    if (const Broadcast *b = e.as<Broadcast>()) {
        return is_negative_const(b->value);
    }
    return false;
}

bool is_zero(Expr e) {
    if (const IntImm *int_imm = e.as<IntImm>()) return int_imm->value == 0;
    if (const FloatImm *float_imm = e.as<FloatImm>()) return float_imm->value == 0.0f;
    if (const Cast *c = e.as<Cast>()) return is_zero(c->value);
    if (const Broadcast *b = e.as<Broadcast>()) return is_zero(b->value);
    return false;
}
        
bool is_one(Expr e) {
    if (const IntImm *int_imm = e.as<IntImm>()) return int_imm->value == 1;
    if (const FloatImm *float_imm = e.as<FloatImm>()) return float_imm->value == 1.0f;
    if (const Cast *c = e.as<Cast>()) return is_one(c->value);
    if (const Broadcast *b = e.as<Broadcast>()) return is_one(b->value);
    return false;
}

bool is_two(Expr e) {
    if (const IntImm *int_imm = e.as<IntImm>()) return int_imm->value == 2;
    if (const FloatImm *float_imm = e.as<FloatImm>()) return float_imm->value == 2.0f;
    if (const Cast *c = e.as<Cast>()) return is_two(c->value);
    if (const Broadcast *b = e.as<Broadcast>()) return is_two(b->value);
    return false;
}


int int_cast_constant(Type t, int val) {
    // Unsigned of less than 32 bits is masked to select the appropriate bits
    if (t.is_uint()) {
        if (t.bits < 32) {
            val = val & ((((unsigned int) 1) << t.bits) - 1);
        }
    }
    else if (t.is_int()) {
        if (t.bits < 32) {
            // sign extend the lower bits
            val = ((val << (32 - t.bits)) >> (32 - t.bits));
        }
    }
    else {
        assert(0 && "Cast of integer to non-integer not available here");
    }
    return val;
}

Expr make_const(Type t, int val) {
    if (t == Int(32)) return val;
    if (t == Float(32)) return (float)val;
    if (t.is_vector()) {
        return new Broadcast(make_const(t.element_of(), val), t.width);
    }
    // When constructing cast integer constants, use the canonical representation.
    if (t.is_int() || t.is_uint()) {
        val = int_cast_constant(t, val);
    }
    return new Cast(t, val);
}

Expr make_infinity(Type t, int count) { //LH
    //if (t.is_vector()) {
    //    return new Broadcast(make_infinity(t.element_of(), count), t.width);
    //}
    return new Infinity(t, count); // Infinity is native to all types, including vectors.
}
        
Expr make_bool(bool val, int w) {
    return make_const(UInt(1, w), val);
}

Expr make_zero(Type t) {
    return make_const(t, 0);
}
        
Expr make_one(Type t) {
    return make_const(t, 1);
}

Expr make_two(Type t) {
    return make_const(t, 2);
}
    
Expr const_true(int w) {
    return make_one(UInt(1, w));
}

Expr const_false(int w) {
    return make_zero(UInt(1, w));
}


void check_defined(const Expr &a, const std::string &op, const Expr &b) {
    if (a.defined() && b.defined()) return;
    std::cerr << "Undefined operand: " << a << " " << op << " " << b << std::endl;
    assert(false && "Undefined operand");
}

void check_defined(const std::string &op, const Expr &a, const Expr &b) {
    if (a.defined() && b.defined()) return;
    std::cerr << "Undefined argument: " << op << "(" << a << ", " << b << ")" << std::endl;
    assert(false && "Undefined argument");
}

void match_types(Expr &a, Expr &b) {
    if (a.type() == b.type()) return;
    
    // First widen to match
    if (a.type().is_scalar() && b.type().is_vector()) {
        a = new Broadcast(a, b.type().width);
    } else if (a.type().is_vector() && b.type().is_scalar()) {
        b = new Broadcast(b, a.type().width);
    } else {
        assert(a.type().width == b.type().width && "Can't match types of differing widths");
    }

    Type ta = a.type(), tb = b.type();

    const Infinity *inf_a = a.as<Infinity>();
    const Infinity *inf_b = b.as<Infinity>();
    if (inf_a) {
        // Type of Infinity is weak and gives way to other type.
        a = cast(b.type(),a);
    } else if (inf_b) {
        b = cast(a.type(),b);
    } else if (!ta.is_float() && tb.is_float()) {
        // int(a) * float(b) -> float(b)
        // uint(a) * float(b) -> float(b)        
        a = cast(tb, a);
    } else if (ta.is_float() && !tb.is_float()) {
        b = cast(ta, b);
    } else if (ta.is_float() && tb.is_float()) {    
        // float(a) * float(b) -> float(max(a, b))
        if (ta.bits > tb.bits) b = cast(ta, b);
        else a = cast(tb, a); 
    } else if (!ta.is_float() && !tb.is_float() && is_const(b)) {
        // (u)int(a) * (u)intImm(b) -> int(a)
        b = cast(ta, b); 
    } else if (!tb.is_float() && !ta.is_float() && is_const(a)) {
        a = cast(tb, a); 
    } else if (ta.is_uint() && tb.is_uint()) {
        // uint(a) * uint(b) -> uint(max(a, b))        
        if (ta.bits > tb.bits) b = cast(ta, b);
        else a = cast(tb, a);
    } else if (!ta.is_float() && !tb.is_float()) {
        // int(a) * (u)int(b) -> int(max(a, b))
        int bits = std::max(ta.bits, tb.bits);
        a = cast(Int(bits), a);
        b = cast(Int(bits), b);
    } else {
        std::cerr << "Could not match types: " << ta << ", " << tb << std::endl;
        assert(false && "Failed type coercion");    
    }
}

/* Recognise Infinity node and return count.  If not infinity, return zero.
 * Handles Cast and Broadcast of Infinity nodes. */
int infinity_count(Expr e) {
    const Infinity *inf = e.as<Infinity>();
    const Cast *cast = e.as<Cast>();
    const Broadcast *broadcast = e.as<Broadcast>();
    
    if (broadcast) return infinity_count(broadcast->value);
    if (cast) return infinity_count(cast->value);
    if (inf) return inf->count;
    return 0;
}


}
}
