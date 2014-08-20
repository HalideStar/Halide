#ifndef HALIDE_CLAMP_H
#define HALIDE_CLAMP_H

// This include file is included in IR.h.  Do not inclucde this file explicitly; use IR.h instead.

#include "HalideFeatures.h"

namespace Halide {
namespace Internal {
/** Clamp and related functions that restrict the range of an expression.
 * The output value is restricted to the inclusive interval (min,max).
 */
#ifdef HALIDE_CLAMP_NODE
struct Clamp : public ExprNode<Clamp> {
    // a, min and max are defined for all clamp-like operations.
    // a is the expression being clamped, min and max are the range limits inclusive.
	Expr a, min, max;
    // p1 is an extra parameter that may be used for some clamp-like operations.
    // Tile: p1 is the tile width.
    Expr p1;
    // None: No clamping is applied at all.  This no-op is used for domain inference because
    //   it represents Border::none which has the semantics that the computable domain becomes a copy
    //   of the valid domain.
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
	typedef enum {None, Replicate, Wrap, Reflect, Reflect101, Tile} ClampType;
	ClampType clamptype;
    
    // Retain the bounds of the expression a as determined by bounds inference.
    // mutable so that this can be done as a side effect of bounds inference
    //mutable Expr min_a, max_a;
	
public:
	static Expr make(ClampType t, Expr a, Expr min = Expr(0), Expr max = Expr(0), Expr p1 = Expr(0)) {
		assert(a.defined() && "Clamp of undefined");
		assert(min.defined() && "Clamp of undefined");
		assert(max.defined() && "Clamp of undefined");
		assert(min.type() == a.type() && "Clamp of mismatched types");
		assert(max.type() == a.type() && "Clamp of mismatched types");
		// Even if the clamp type is not Tile, we require a defined tile
		// expression - makes it easier to walk the tree.  The expression is ignored.
		assert(p1.defined() && "Clamp of undefined");
		if (t == Tile) {
			assert(p1.type() == a.type() && "Clamp of mismatched types");
		}
		
		Clamp *node = new Clamp;
		node->type = a.type();
		node->clamptype = t;
		node->a = a;
		node->min = min;
		node->max = max;
		node->p1 = p1;
		return node;
	}
};
#endif

}
}
#endif
