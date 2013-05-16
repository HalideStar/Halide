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

/** Perform loop partition optimizations on a halide
 * statement. 
 */
Stmt loop_partition(Stmt s);

void loop_partition_test();
bool is_effective_partition(Stmt s);
}
}

#endif
