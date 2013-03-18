#ifndef HALIDE_CLAMP_H
#define HALIDE_CLAMP_H

// This include file is included in IR.h.  Do not inclucde this file explicitly; use IR.h instead.

namespace Halide {
namespace Internal {
/** Clamp and related functions that restrict the range of an expression.
 * The output value is restricted to the inclusive interval (min,max).
 */
struct Clamp : public ExprNode<Clamp> {
    // a, min and max are defined for all clamp-like operations.
    // a is the expression being clamped, min and max are the range limits inclusive.
	Expr a, min, max;
    // p1 is an extra parameter that may be used for some clamp-like operations.
    // Tile: p1 is the tile width.
    Expr p1;
	// Replicate: If value is out of range, restrict it to the range. Implements clamp() function and
    //   when applied to image indices implements border pixel replication.
    //   On image indices: |abcd| becomes ...aaaa|abcd|dddd...
	// Wrap: If value is out of range, wrap it into the range. 
    //   On image indices: |abcd| becomes ...abcdabcd|abcd|abcdabcd...
	// Reflect: If value is out of range, reflect about the boundaries until it falls in the range.
    //   On image indices: |abcd| becomes ...abcddcba|abcd|dcbaabcd...
	// Reflect101: Reflect about the boundaries, but the min and max values are not repeated adjacent to themselves.
    //   On image indices: |abcd| becomes ...abcdcb|abcd|cbabcd...
	// Tile: Replicate a portion of the range outside the range.
    //   On image indices with tile width of 2: |abcdef| becomes ...ababab|abcdef|efefef...
	typedef enum {Replicate, Wrap, Reflect, Reflect101, Tile} ClampType;
	ClampType clamptype;
    
    // Retain the bounds of the expression a as determined by bounds inference.
    // mutable so that this can be done as a side effect of bounds inference
    //mutable Expr min_a, max_a;
	
private:
	void constructor();

public:
	Clamp(ClampType _t, Expr _a, Expr _min, Expr _max, Expr _p1): 
		ExprNode<Clamp>(_a.type()), a(_a), min(_min), max(_max), p1(_p1), clamptype(_t) {
		constructor();
	}
	Clamp(ClampType _t, Expr _a, Expr _min, Expr _max): 
		ExprNode<Clamp>(_a.type()), a(_a), min(_min), max(_max), p1(Expr(0)), clamptype(_t) {
		assert(clamptype != Tile && "Tile clamp without tile expression");
		constructor();
	}
};


}
}
#endif