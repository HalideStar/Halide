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
#include "Statistics.h"
#include "Interval.h"
#include "IRLazyScope.h"
#include "Options.h"
#include "BoundsSimplify.h"

using namespace Halide;
using namespace Halide::Internal;

int main(int argc, const char **argv) {
    IRPrinter::test();
    
    global_options.mutator_cache = false;

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
    
    interval_test();
    domain_inference_test();
    lower_clamp_test();
    border_test();
    interval_analysis_test();
    solver_test();
    loop_partition_test();
    lazy_scope_test();
    bounds_simplify_test();
    
    std::cout << "Compiler Statistics:\n";
    std::cout << global_statistics;
    return 0;
}
