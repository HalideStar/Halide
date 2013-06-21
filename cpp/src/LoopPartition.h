#ifndef HALIDE_LOOP_PARTITION_H
#define HALIDE_LOOP_PARTITION_H

/** \file
 *
 * Defines the loop partitioning lowering optimization pass, which avoids
 * expensive tests inside loops by partitioning and specialising the loops.
 */

#include "IR.h"
#include <map>

namespace Halide {
namespace Internal {

/** Perform index-set loop split optimizations on a halide
 * statement. 
 */
Stmt loop_split(Stmt s);

void loop_split_test();
bool is_effective_loop_split(Stmt s);
}
}

#endif
