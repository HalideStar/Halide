#ifndef HALIDE_LOWERBORDER_H
#define HALIDE_LOWERBORDER_H

//#define LOWER_CLAMP 140
//#define LOWER_CLAMP 290
#define LOWER_CLAMP 490

namespace Halide {
namespace Internal {

Expr lower_clamp(Expr e);
Stmt lower_clamp(Stmt s);

void lower_clamp_test();
}
}
#endif
