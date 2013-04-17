#include "LoopPartition.h"
#include "IntervalAnalysis.h"
#include "IRMutator.h"
#include "IROperator.h"
#include "IRPrinter.h"
#include "IRRewriter.h"
#include "Scope.h"
#include "Log.h"
#include "Substitute.h"

namespace Halide {
namespace Internal {

using std::string;
using std::map;

// 


// Perform loop partition optimisation for all For loops
class LoopPartition : public IRMutator {
    //const map<string, Function> &env;

    using IRMutator::visit;

    void visit(const For *op) {
        // Only apply optimisation to serial For loops.
        // Parallel loops may also be eligible for optimisation, but not yet handled.
        // Vectorised loops are not eligible, and unrolled loops should be fully
        // optimised for each iteration due to the unrolling.
        Stmt new_body = mutate(op->body);
        if ((op->for_type == For::Serial || op->for_type == For::Parallel) && 
            (op->partition_begin > 0 || op->partition_end > 0)) {
            //log(0) << "Found serial for loop \n" << Stmt(op) << "\n";
            // Greedy allocation of up to partition_begin loop iterations to before.
            Expr before_min = op->min;
            Expr before_extent = min(op->partition_begin, op->extent);
            // Greedy allocation of up to partition_end loop iterations to after.
            Expr main_min = before_min + op->partition_begin; // May be BIG
            Expr main_extent = op->extent - op->partition_begin - op->partition_end; // May be negative
            Expr after_min = main_min + max(0,main_extent);
            Expr after_extent = min(op->partition_end, op->extent - before_extent);
            // Now generate the partitioned loops.
            // Mark them by using negative numbers for the partition information.
            Stmt before = new For(op->name, before_min, before_extent, op->for_type, -2, -2, op->body);
            Stmt main = new For(op->name, main_min, main_extent, op->for_type, -3, -3, new_body);
            Stmt after = new For(op->name, after_min, after_extent, op->for_type, -4, -4, op->body);
            stmt = new Block(new Block(before,main),after);
            if (stmt.same_as(op)) {
                stmt = op;
            }
        } else {
            if (new_body.same_as(op->body)) {
                stmt = op;
            } else {
                stmt = new For(*op, op->min, op->extent, new_body);
            }
        }
    }
public:
    LoopPartition() {}

};

Stmt loop_partition(Stmt s) {
    return LoopPartition().mutate(s);
}

}
}
