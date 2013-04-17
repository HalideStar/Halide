#include "IR.h"
#include "IRPrinter.h"
#include "CodeGen_X86.h"
#include "CodeGen_C.h"
#include "Func.h"
#include "Simplify.h"
#include "Bounds.h"
#include "Lower.h"
#include "IRMatch.h"
#include "Deinterleave.h"
#include "ModulusRemainder.h"

#include "DomainInference.h"
#include "LowerClamp.h"
#include "Border.h"
#include "IntervalAnalysis.h"
#include "Solver.h"
#include "LoopPartition.h"

using namespace Halide;
using namespace Halide::Internal;

int main(int argc, const char **argv) {
    IRPrinter::test();

    #ifdef __i386__
    CodeGen_X86::test();
    #endif
    
    CodeGen_C::test();
    simplify_test();
    bounds_test();
    lower_test();
    Func::test();
    expr_match_test();
    deinterleave_vector_test();
    modulus_remainder_test();
    
    domain_inference_test();
    lower_clamp_test();
    border_test();
    interval_analysis_test();
    solver_test();
    loop_partition_test();
    return 0;
}
