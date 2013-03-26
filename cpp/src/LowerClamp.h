#ifndef HALIDE_LOWERBORDER_H
#define HALIDE_LOWERBORDER_H

#define LOWER_CLAMP_LATE 0

namespace Halide {
namespace Internal {

Expr lower_clamp(Expr e);
Stmt lower_clamp(Stmt s);

void lower_clamp_test();
}
}
#endif
