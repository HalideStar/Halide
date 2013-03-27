#include "LoopPartition.h"
#include "IntervalAnalysis.h"
#include "IRMutator.h"
#include "IROperator.h"
#include "IRRewriter.h"
#include "Scope.h"
#include "Log.h"
#include "Substitute.h"

namespace Halide {
namespace Internal {

using std::string;
using std::map;

# if 0
// Does an expression depend on a particular variable?
class ExprDependsOnVar : public IRVisitor {    
    using IRVisitor::visit;

    void visit(const Variable *op) {
        if (op->name == var) result = true;
    }

    void visit(const Let *op) {
        op->value.accept(this);
        // The name might be hidden within the body of the let, in
        // which case there's no point descending.
        if (op->name != var) {
            op->body.accept(this);
        }
    }
public:

    bool result;
    string var;

    ExprDependsOnVar(Expr e, string v) : result(false), var(v) {        
        if (e.defined()) e.accept(this);
    }
};

// Perform sliding window optimization for a function over a
// particular serial for loop
class LoopPartitionOnFunctionAndLoop : public IRMutator {
    Function func;
    string loop_var;
    Expr loop_min;
    Scope<Expr> scope;

    using IRMutator::visit;

    void visit(const Pipeline *op) {
        if (op->name != func.name()) {
            IRMutator::visit(op);
        } else {
            
            // We're interested in the case where exactly one of the
            // mins of the buffer depends on the loop_var, and none of
            // the extents do.
            string dim = "";
            Expr min, extent;

            for (size_t i = 0; i < func.args().size(); i++) {
                string min_name = func.name() + "." + func.args()[i] + ".min";
                string extent_name = func.name() + "." + func.args()[i] + ".extent";
                assert(scope.contains(min_name) && scope.contains(extent_name));
                Expr this_min = scope.get(min_name);
                Expr this_extent = scope.get(extent_name);

                if (ExprDependsOnVar(this_extent, loop_var).result) {
                    min = Expr();
                    extent = Expr();
                    break;
                }

                if (ExprDependsOnVar(this_min, loop_var).result) {
                    if (min.defined()) {
                        min = Expr();
                        extent = Expr();
                        break;
                    } else {
                        dim = func.args()[i];
                        min = this_min;
                        extent = this_extent;
                    }
                }
            }

            if (min.defined()) {
                // Ok, we've isolated a function, a dimension to slide along, and loop variable to slide over
                log(2) << "Sliding " << func.name() << " over dimension " << dim << " along loop variable " << loop_var << "\n";
                
                Expr loop_var_expr = new Variable(Int(32), loop_var);
                Expr steady_state = loop_var_expr > loop_min;

                // The new min is one beyond the max we reached on the last loop iteration
                Expr new_min = substitute(loop_var, loop_var_expr - 1, min + extent);
                // The new extent is the old extent shrunk by how much we trimmed off the min
                Expr new_extent = extent + min - new_min;

                new_min = new Select(steady_state, new_min, min);
                new_extent = new Select(steady_state, new_extent, extent);

                stmt = new LetStmt(func.name() + "." + dim + ".extent", new_extent, op);
                stmt = new LetStmt(func.name() + "." + dim + ".min", new_min, stmt);

            } else {
                log(2) << "Could not perform sliding window optimization of " << func.name() << " over " << loop_var << "\n";
                stmt = op;
            }


        }
    }

    void visit(const LetStmt *op) {
        scope.push(op->name, op->value);
        Stmt new_body = mutate(op->body);
        if (new_body.same_as(op->body)) {
            stmt = op;
        } else {
            stmt = new LetStmt(op->name, op->value, new_body);
        }
        scope.pop(op->name);
    }

public:
    LoopPartitionOnFunctionAndLoop(Function f, string v, Expr v_min) : func(f), loop_var(v), loop_min(v_min) {}
};

// Perform sliding window optimization for a particular function
class LoopPartitionOnFunction : public IRMutator {
    Function func;

    using IRMutator::visit;

    void visit(const For *op) {
        Stmt new_body = mutate(op->body);

        if (op->for_type == For::Serial || op->for_type == For::Unrolled) {
            new_body = LoopPartitionOnFunctionAndLoop(func, op->name, op->min).mutate(new_body);
        }

        if (new_body.same_as(op->body)) {
            stmt = op;
        } else {
            stmt = new For(op->name, op->min, op->extent, op->for_type, new_body);
        }
    }

public:
    LoopPartitionOnFunction(Function f) : func(f) {}
};
#endif

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
        if (op->for_type == For::Serial) {
            // Heuristic solution.  Assume that it might help to specialise the first
            // N and last N iterations.
            int N = 5;
            Expr before_min = op->min;
            Expr before_extent = min(N, op->extent);
            Expr main_min = before_min + before_extent;
            Expr main_extent = max(0, op->extent - before_extent - N);
            Expr after_min = main_min + main_extent;
            Expr after_extent = op->extent - before_extent - main_extent;
           // Now generate the partitioned loops.
            Stmt before = new For(op->name, before_min, before_extent, op->for_type, op->body);
            Stmt main = new For(op->name, main_min, main_extent, op->for_type, new_body);
            Stmt after = new For(op->name, after_min, after_extent, op->for_type, op->body);
            stmt = new Block(new Block(before,main),after);
            if (stmt.same_as(op)) {
                stmt = op;
            }
        } else {
            if (new_body.same_as(op->body)) {
                stmt = op;
            } else {
                stmt = new For(op->name, op->min, op->extent, op->for_type, new_body);
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
