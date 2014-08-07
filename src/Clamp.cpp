#include "IR.h"

namespace Halide {
namespace Internal {

void Clamp::constructor() {
    assert(a.defined() && "Clamp of undefined");
    assert(min.defined() && "Clamp of undefined");
    assert(max.defined() && "Clamp of undefined");
    assert(min.type() == type && "Clamp of mismatched types");
    assert(max.type() == type && "Clamp of mismatched types");
    // Even if the clamp type is not Tile, we require a defined tile
    // expression - makes it easier to walk the tree.  The expression is ignored.
    assert(p1.defined() && "Clamp of undefined");
    if (clamptype == Tile) {
        assert(p1.type() == type && "Clamp of mismatched types");
    }
}

}
}
