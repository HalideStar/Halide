#ifndef HALIDE_DOMAININFERENCE_H
#define HALIDE_DOMAININFERENCE_H

#include "IR.h"
#include "Function.h"

namespace Halide {
namespace Internal {
void domain_inference(Expr e);
}
}
#endif