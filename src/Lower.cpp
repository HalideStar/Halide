#include "Lower.h"
#include "IROperator.h"
#include "Substitute.h"
#include "Function.h"
#include "Scope.h"
#include "Bounds.h"
#include "Simplify.h"
#include "IRLazyScope.h"
#include "IREquality.h"
#include "IRPrinter.h"
#include "Log.h"
#include <iostream>
#include <set>
#include <sstream>
#include "Tracing.h"
#include "StorageFlattening.h"
#include "BoundsInference.h"
#include "VectorizeLoops.h"
#include "UnrollLoops.h"
#include "SlidingWindow.h"
#include "StorageFolding.h"
#include "RemoveTrivialForLoops.h"
#include "Deinterleave.h"
#include "DebugToFile.h"
#include "EarlyFree.h"
#include "UniquifyVariableNames.h"
// LHHACK: remove_dead_lets called in loop split
#include "RemoveDeadLets.h"

#include "LowerClamp.h"
#include "LoopPartition.h"
#include "BoundsSimplify.h"
#include "Statistics.h"
#include "CodeLogger.h"

namespace Halide {
namespace Internal {

using std::set;
using std::ostringstream;
using std::string;
using std::vector;
using std::map;
using std::pair;
using std::make_pair;

void lower_test() {

    Func f("f"), g("g"), h("h");
    Var x("x"), y("y"), xi, xo, yi, yo;
    
    h(x, y) = x-y;    
    g(x, y) = h(x+1, y) + h(x-1, y);
    f(x, y) = g(x, y-1) + g(x, y+1);
    

    //f.split(x, xo, xi, 4).vectorize(xi).parallel(xo);
    //f.compute_root();       

    //g.split(y, yo, yi, 2).unroll(yi);;
    g.store_at(f, y).compute_at(f, x);
    h.store_at(f, y).compute_at(f, y);

    Stmt result = lower(f.function());

    assert(result.defined() && "Lowering returned trivial function");

    std::cout << "Lowering test passed" << std::endl;
}

// Prefix all names in an expression with some string.
class QualifyExpr : public IRMutator {
    string prefix;

    using IRMutator::visit;

    void visit(const Variable *v) {
        if (v->param.defined()) {
            expr = v;
        } else {
            expr = Variable::make(v->type, prefix + v->name, v->reduction_domain);
        }
    }
    void visit(const Let *op) {
        Expr value = mutate(op->value);
        Expr body = mutate(op->body);
        expr = Let::make(prefix + op->name, value, body);
    }
public:
    QualifyExpr(string p) : prefix(p) {}
};

Expr qualify_expr(string prefix, Expr value) {
    QualifyExpr q(prefix);
    return q.mutate(value);
}

// Build a loop nest about a provide node using a schedule
Stmt build_provide_loop_nest(string buffer, string prefix, vector<Expr> site, Expr value, const Schedule &s) {
    // We'll build it from inside out, starting from a store node,
    // then wrapping it in for loops.
            
    // Make the (multi-dimensional) store node
    Stmt stmt = Provide::make(buffer, value, site);
            
    // Define the function args in terms of the loop variables using the splits
    for (size_t i = 0; i < s.splits.size(); i++) {
        const Schedule::Split &split = s.splits[i];
        Expr outer = Variable::make(Int(32), prefix + split.outer);
        if (!split.is_rename) {
            Expr inner = Variable::make(Int(32), prefix + split.inner);
            Expr old_min = Variable::make(Int(32), prefix + split.old_var + ".min");
            // stmt = LetStmt::make(prefix + split.old_var, outer * split.factor + inner + old_min, stmt);
            stmt = substitute(prefix + split.old_var, outer * split.factor + inner + old_min, stmt);
        } else {
            stmt = substitute(prefix + split.old_var, outer, stmt);
        }
    }
       
    // Build the loop nest
    for (size_t i = 0; i < s.dims.size(); i++) {
        const Schedule::Dim &dim = s.dims[i];
        Expr min = Variable::make(Int(32), prefix + dim.var + ".min");
        Expr extent = Variable::make(Int(32), prefix + dim.var + ".extent");
        // Loop splitting information. If not defined for the particular variable, then look for
        // a function-level option.
        LoopSplitInfo loop_split = dim.loop_split;
        if (! loop_split.defined() && s.loop_split_compile.auto_split != LoopSplitInfo::Undefined) loop_split = LoopSplitInfo(s.loop_split_compile.auto_split);
        //log(0) << "Compile " << s.loop_split_compile.auto_split << "  settings " << s.loop_split_settings.auto_split << "\n";
        stmt = For::make(prefix + dim.var, min, extent, dim.for_type, loop_split, stmt); // index-set loop splitting information
    }

    // Define the bounds on the split dimensions using the bounds
    // on the function args
    for (size_t i = s.splits.size(); i > 0; i--) {
        const Schedule::Split &split = s.splits[i-1];
        Expr old_var_extent = Variable::make(Int(32), prefix + split.old_var + ".extent");
        Expr old_var_min = Variable::make(Int(32), prefix + split.old_var + ".min");
        if (!split.is_rename) {
            Expr inner_extent = split.factor;
            Expr outer_extent = (old_var_extent + split.factor - 1)/split.factor;
            stmt = LetStmt::make(prefix + split.inner + ".min", 0, stmt);
            stmt = LetStmt::make(prefix + split.inner + ".extent", inner_extent, stmt);
            stmt = LetStmt::make(prefix + split.outer + ".min", 0, stmt);
            stmt = LetStmt::make(prefix + split.outer + ".extent", outer_extent, stmt);            
        } else {
            stmt = LetStmt::make(prefix + split.outer + ".min", old_var_min, stmt);
            stmt = LetStmt::make(prefix + split.outer + ".extent", old_var_extent, stmt);
        }
    }

    return stmt;
}

// Turn a function into a loop nest that computes it. It will
// refer to external vars of the form function_name.arg_name.min
// and function_name.arg_name.extent to define the bounds over
// which it should be realized. It will compute at least those
// bounds (depending on splits, it may compute more). This loop
// won't do any allocation.
Stmt build_produce(Function f) {

    string prefix = f.name() + ".";

    // Compute the site to store to as the function args
    vector<Expr> site;
    Expr value = qualify_expr(prefix, f.value());   

    for (size_t i = 0; i < f.args().size(); i++) {
        site.push_back(Variable::make(Int(32), f.name() + "." + f.args()[i]));
    }

    return build_provide_loop_nest(f.name(), prefix, site, value, f.schedule());
}

// Build the loop nest that updates a function (assuming it's a reduction).
Stmt build_update(Function f) {
    if (!f.is_reduction()) return Stmt();

    string prefix = f.name() + ".";

    vector<Expr> site;
    Expr value = qualify_expr(prefix, f.reduction_value());

    for (size_t i = 0; i < f.reduction_args().size(); i++) {
        site.push_back(qualify_expr(prefix, f.reduction_args()[i]));
        log(2) << "Reduction site " << i << " = " << site[i] << "\n";
    }

    Stmt loop = build_provide_loop_nest(f.name(), prefix, site, value, f.reduction_schedule());

    // Now define the bounds on the reduction domain
    const vector<ReductionVariable> &dom = f.reduction_domain().domain();
    for (size_t i = 0; i < dom.size(); i++) {
        string p = prefix + dom[i].var;
        loop = LetStmt::make(p + ".min", dom[i].min, loop);
        loop = LetStmt::make(p + ".extent", dom[i].extent, loop);
    }

    return loop;
}

pair<Stmt, Stmt> build_realization(Function func) {
    Stmt produce = build_produce(func);
    Stmt update = build_update(func);
    
    if (update.defined()) {
        // Expand the bounds computed in the produce step
        // using the bounds read in the update step. This is
        // necessary because later bounds inference does not
        // consider the bounds read during an update step
        Region bounds = region_called(update, func.name());
        if (!bounds.empty()) {
            assert(bounds.size() == func.args().size());
            // Expand the region to be computed using the region read in the update step
            for (size_t i = 0; i < bounds.size(); i++) {                        
                string var = func.name() + "." + func.args()[i];
                Expr update_min = Variable::make(Int(32), var + ".update_min");
                Expr update_extent = Variable::make(Int(32), var + ".update_extent");
                Expr consume_min = Variable::make(Int(32), var + ".min");
                Expr consume_extent = Variable::make(Int(32), var + ".extent");
                Expr init_min = Min::make(update_min, consume_min);
                Expr init_max_plus_one = Max::make(update_min + update_extent, consume_min + consume_extent);
                Expr init_extent = init_max_plus_one - init_min;
                produce = LetStmt::make(var + ".min", init_min, produce);
                produce = LetStmt::make(var + ".extent", init_extent, produce);
            }
            
            // Define the region read during the update step
            for (size_t i = 0; i < bounds.size(); i++) {
                string var = func.name() + "." + func.args()[i];
                produce = LetStmt::make(var + ".update_min", bounds[i].min, produce);
                produce = LetStmt::make(var + ".update_extent", bounds[i].extent, produce);
            }
        }
    }    
    return make_pair(produce, update);
}

// A schedule may include explicit bounds on some dimension. This
// injects let statements that set those bounds, and assertions that
// check that those bounds are sufficiently large to cover the
// inferred bounds required.
Stmt inject_explicit_bounds(Stmt body, Function func) {           
    // Inject any explicit bounds
    for (size_t i = 0; i < func.schedule().bounds.size(); i++) {
        Schedule::Bound b = func.schedule().bounds[i];
        string min_name = func.name() + "." + b.var + ".min";
        string extent_name = func.name() + "." + b.var + ".extent";
        Expr min_var = Variable::make(Int(32), min_name);
        Expr extent_var = Variable::make(Int(32), extent_name);
        Expr check = (b.min <= min_var) && ((b.min + b.extent) >= (min_var + extent_var)); 
        string error_msg = "Bounds given for " + b.var + " in " + func.name() + " don't cover required region";
        body = Block::make(AssertStmt::make(check, error_msg),
                         LetStmt::make(min_name, b.min, 
                                     LetStmt::make(extent_name, b.extent, body)));
    }            
    return body;
}

class IsCalledInStmt : public IRVisitor {
    string func;

    using IRVisitor::visit;

    void visit(const Call *op) {
        IRVisitor::visit(op);
        if (op->name == func) result = true;
    }

public:
    bool result;
    IsCalledInStmt(Function f) : func(f.name()), result(false) {
    }
    
};

bool function_is_called_in_stmt(Function f, Stmt s) {
    IsCalledInStmt is_called(f);
    s.accept(&is_called);
    return is_called.result;
}

// Inject the allocation and realization of a function into an
// existing loop nest using its schedule
class InjectRealization : public IRMutator {
public:
    const Function &func;
    bool found_store_level, found_compute_level;
    InjectRealization(const Function &f) : func(f), found_store_level(false), found_compute_level(false) {}    

private:
    Stmt build_pipeline(Stmt s) {
        pair<Stmt, Stmt> realization = build_realization(func);
        return Pipeline::make(func.name(), realization.first, realization.second, s);
    }

    Stmt build_realize(Stmt s) {
        // The allocate should cover everything touched below this point        
        //Region bounds = region_touched(s, func.name());
        
        /* The following works if the provide steps of a realization
         * always covers the region that will be used */
        Region bounds = region_provided(s, func.name());

        /* The following would work if things were only ever computed
         * exactly to cover the region read. Loop splitting (which
         * rounds things up, and reductions spraying writes
         * everywhere, both break this assumption */

        /*
        Region bounds;
        for (size_t i = 0; i < func.args().size(); i++) {
            Expr min = Variable::make(Int(32), func.name() + "." + func.args()[i] + ".min");
            Expr extent = Variable::make(Int(32), func.name() + "." + func.args()[i] + ".extent");
            bounds.push_back(Range(min, extent));
        }
        */

        for (size_t i = 0; i < bounds.size(); i++) {
            if (!bounds[i].min.defined()) {
                std::cerr << "Use of " << func.name() << " is unbounded below in dimension " 
                          << func.args()[i] << " in the following statement:\n" << s << "\n";
                assert(false);
            }
            if (!bounds[i].extent.defined()) {
                std::cerr << "Use of " << func.name() << " is unbounded above in dimension " 
                          << func.args()[i] << " in the following statement:\n" << s << "\n";
                assert(false);
            }
        }

        // Change the body of the for loop to do an allocation
        s = Realize::make(func.name(), func.value().type(), bounds, s);
        
        return inject_explicit_bounds(s, func);        
    }

    using IRMutator::visit;

    virtual void visit(const For *for_loop) {            
        log(3) << "InjectRealization of " << func.name() << " entering for loop over " << for_loop->name << "\n";
        const Schedule::LoopLevel &compute_level = func.schedule().compute_level;
        const Schedule::LoopLevel &store_level = func.schedule().store_level;

        Stmt body = for_loop->body;

        // Can't schedule things inside a vector for loop
        if (for_loop->for_type != For::Vectorized) {
            body = mutate(for_loop->body);
        } else if (func.is_reduction() && 
                   func.schedule().compute_level.is_inline() &&
                   function_is_called_in_stmt(func, for_loop)) {
            // If we're trying to inline a reduction, schedule it here and bail out            
            log(2) << "Injecting realization of " << func.name() << " around node " << Stmt(for_loop) << "\n";
            stmt = build_realize(build_pipeline(for_loop));
            found_store_level = found_compute_level = true;
            return;
        }

        if (compute_level.match(for_loop->name)) {
            log(3) << "Found compute level\n";
            if (function_is_called_in_stmt(func, body)) {
                body = build_pipeline(body);
            }
            found_compute_level = true;
        } 

        if (store_level.match(for_loop->name)) {
            log(3) << "Found store level\n";
            assert(found_compute_level && 
                   "The compute loop level was not found within the store loop level!");

            if (function_is_called_in_stmt(func, body)) {
                body = build_realize(body);
            }

            found_store_level = true;
        }


        if (body.same_as(for_loop->body)) {
            stmt = for_loop;
        } else {
            stmt = For::make(for_loop, 
                           for_loop->min, 
                           for_loop->extent, 
                           body);
        }
    }
    
    // If we're an inline reduction, we may need to inject a realization here
    virtual void visit(const Provide *op) {               
        if (op->name != func.name() && 
            func.is_reduction() && 
            func.schedule().compute_level.is_inline() &&
            function_is_called_in_stmt(func, op)) {
            log(2) << "Injecting realization of " << func.name() << " around node " << Stmt(op) << "\n";
            stmt = build_realize(build_pipeline(op));
            found_store_level = found_compute_level = true;
        } else {
            stmt = op;
        }
    }

};

class InlineFunction : public IRMutator {
    Function func;
public:
    InlineFunction(Function f) : func(f) {
        assert(!f.is_reduction());
    }
private:
    using IRMutator::visit;

    void visit(const Call *op) {        
        // std::cout << "Found call to " << op->name << endl;
        if (op->name == func.name()) {
            // Mutate the args
            vector<Expr> args(op->args.size());
            for (size_t i = 0; i < args.size(); i++) {
                args[i] = mutate(op->args[i]);
            }
            // Grab the body
            Expr body = qualify_expr(func.name() + ".", func.value());

            
            // Bind the args using Let nodes

            /*
            assert(args.size() == func.args().size());
            for (size_t i = 0; i < args.size(); i++) {
                body = Let::make(func.name() + "." + func.args()[i], 
                               args[i], 
                               body);
            }
            */
            
            
            // Paste in the args directly - introducing too many let
            // statements messes up all our peephole matching
            
            if (func.args().size() < args.size()) { //LH
                std::cout << "Too many arguments for call to " << func.name() << "  Expect " << func.args().size() << " got " << args.size() << "\n";
            }
            for (size_t i = 0; i < args.size(); i++) {
                body = substitute(func.name() + "." + func.args()[i], 
                                  args[i], body);
            
            }
            
            
            expr = body;
        } else {
            IRMutator::visit(op);
        }
    }
};

/* Find all the internal halide calls in an expr */
class FindCalls : public IRVisitor {
public:
    map<string, Function> calls;

    using IRVisitor::visit;

    void visit(const Call *call) {                
        IRVisitor::visit(call);
        if (call->call_type == Call::Halide) {
            map<string, Function>::iterator iter = calls.find(call->name);
            if (iter == calls.end()) {
                calls[call->name] = call->func;
            } else {
                assert(iter->second.same_as(call->func) && 
                       "Can't compile a pipeline using multiple functions with same name");
            }
        }
    }
};

/* Find all the internal halide calls in an expr, recursively */
class FindCallsRec : public IRVisitor {
public:
    FindCallsRec(Expr e) {e.accept(this);}
    map<string, Function> calls;

    using IRVisitor::visit;

    void visit(const Call *call) {                
        IRVisitor::visit(call); // Do the default handling to visit the index expressions
        if (call->call_type == Call::Halide) {
            map<string, Function>::iterator iter = calls.find(call->name);
            if (iter == calls.end()) {
                calls[call->name] = call->func;
                // Recursively find all the calls in that function also.
                call->func.value().accept(this);
            } else {
                assert(iter->second.same_as(call->func) && 
                       "Can't compile a pipeline using multiple functions with same name");
            }
        }
    }
};

/* Recursively propagate _all schedule information */
class PropagateScheduleAll : public IRVisitor {
public:
    using IRVisitor::visit;
    map<string, Function> calls;
    
    Function parent; // Parent function for access to schedule as needed
    
    PropagateScheduleAll(Function main_func) : parent(main_func) {}
    
    void propagate_schedule(Function parent, Function child) {
        // Get handle to child's schedule for modifying it
        Schedule &child_s = child.schedule();
        // Default compilation settings to be as specified by the user
        child_s.loop_split_compile = child_s.loop_split_settings;
        // Propagate _all settings from parent where child is undefined
        Schedule &parent_s = parent.schedule();
        if (child_s.loop_split_compile.auto_split_all == LoopSplitInfo::Undefined)
            child_s.loop_split_compile.auto_split_all = parent_s.loop_split_compile.auto_split_all;
        if (child_s.loop_split_compile.split_borders_all == LoopSplitInfo::Undefined)
            child_s.loop_split_compile.split_borders_all = parent_s.loop_split_compile.split_borders_all;
        // Give _all settings effect for child function if ordinary setting is undefined
        if (child_s.loop_split_compile.auto_split == LoopSplitInfo::Undefined)
            child_s.loop_split_compile.auto_split = child_s.loop_split_compile.auto_split_all;
        if (child_s.loop_split_compile.split_borders == LoopSplitInfo::Undefined)
            child_s.loop_split_compile.split_borders = child_s.loop_split_compile.split_borders_all;
        return;
    }

    void visit(const Call *call) {                
        IRVisitor::visit(call); // Do the default handling to visit the index expressions
        if (call->call_type == Call::Halide) {
            // Check that our name is not already associated with a different function
            map<string, Function>::iterator iter = calls.find(call->name);
            if (iter == calls.end()) {
                // Name not found. Add our function to the list.
                calls[call->name] = call->func;
                // Update schedule information in our functions.
                propagate_schedule(parent, call->func);
                // Recursively find all the calls in this function also.
                // Include both value() and reduction_value() expressions.
                {
                    Function outer_parent = parent;
                    parent = call->func;
                    if (call->func.value().defined())
                        call->func.value().accept(this);
                    if (call->func.reduction_value().defined())
                        call->func.reduction_value().accept(this);
                    parent = outer_parent;
                }
            } else {
                assert(iter->second.same_as(call->func) && 
                       "Can't compile a pipeline using multiple functions with same name");
            }
        }
    }
    
    void do_propagate_schedule() {
        // Be sure to apply the _all settings in the parent (main) function
        propagate_schedule(parent, parent);
        if (parent.value().defined())
            parent.value().accept(this);
        if (parent.reduction_value().defined())
            parent.reduction_value().accept(this);
    }
};


void propagate_schedule_all(Function f) {
    PropagateScheduleAll prop(f);
    prop.do_propagate_schedule();
}



/* Find all the externally referenced buffers in a stmt */
class FindBuffers : public IRVisitor {
public:
    struct Result {
        Buffer image;
        Parameter param;
        Type type;
    };

    map<string, Result> buffers;

    using IRVisitor::visit;

    void visit(const Call *op) {
        IRVisitor::visit(op);        
        if (op->image.defined()) {
            Result r;
            r.image = op->image;
            r.type = op->type.element_of();
            buffers[op->name] = r;
        } else if (op->param.defined()) {
            Result r;
            r.param = op->param;
            r.type = op->type.element_of();
            buffers[op->name] = r;
        }
    }
};

void populate_environment(Function f, map<string, Function> &env, bool recursive = true) {    
    map<string, Function>::const_iterator iter = env.find(f.name());
    if (iter != env.end()) {
        assert(iter->second.same_as(f) && 
               "Can't compile a pipeline using multiple functions with same name");
        return;
    }
            
    FindCalls calls;
    f.value().accept(&calls);

    // Consider reductions
    if (f.is_reduction()) {
        f.reduction_value().accept(&calls);
        for (size_t i = 0; i < f.reduction_args().size(); i++) {
            f.reduction_args()[i].accept(&calls);
        }
    }

    if (!recursive) {
        env.insert(calls.calls.begin(), calls.calls.end());
    } else {
        env[f.name()] = f;            

        for (map<string, Function>::const_iterator iter = calls.calls.begin(); 
             iter != calls.calls.end(); ++iter) {
            populate_environment(iter->second, env);                    
        }
    }
}

vector<string> realization_order(string output, const map<string, Function> &env, map<string, set<string> > &graph) {
    // Make a DAG representing the pipeline. Each function maps to the set describing its inputs.
    // Populate the graph
    for (map<string, Function>::const_iterator iter = env.begin(); 
         iter != env.end(); ++iter) {
        map<string, Function> calls;
        populate_environment(iter->second, calls, false);

        for (map<string, Function>::const_iterator j = calls.begin(); 
             j != calls.end(); ++j) {
            graph[iter->first].insert(j->first);
        }
    }

    vector<string> result;
    set<string> result_set;

    while (true) {
        // Find a function not in result_set, for which all its inputs are
        // in result_set. Stop when we reach the output function.
        bool scheduled_something = false;
        for (map<string, Function>::const_iterator iter = env.begin(); 
             iter != env.end(); ++iter) {
            const string &f = iter->first;
            if (result_set.find(f) == result_set.end()) {
                bool good_to_schedule = true;
                const set<string> &inputs = graph[f];
                for (set<string>::const_iterator i = inputs.begin(); 
                     i != inputs.end(); ++i) {
                    if (*i != f && result_set.find(*i) == result_set.end()) {
                        good_to_schedule = false;
                    }
                }

                if (good_to_schedule) {
                    scheduled_something = true;
                    result_set.insert(f);
                    result.push_back(f);
                    if (f == output) return result;
                }
            }
        }
            
        assert(scheduled_something && "Stuck in a loop computing a realization order. Perhaps this pipeline has a loop?");
    }

}

Stmt create_initial_loop_nest(Function f) {
    // Generate initial loop nest    
    pair<Stmt, Stmt> r = build_realization(f);
    Stmt s = r.first;
    if (r.second.defined()) {
        // This must be in a pipeline so that bounds inference understands the update step
        s = Pipeline::make(f.name(), r.first, r.second, AssertStmt::make(const_true(), "Dummy consume step"));
    }
    return inject_explicit_bounds(s, f);
}

Stmt schedule_functions(Stmt s, const vector<string> &order, 
                        const map<string, Function> &env, 
                        const map<string, set<string> > &graph) {

    // Inject a loop over root to give us a scheduling point
    string root_var = Schedule::LoopLevel::root().func + "." + Schedule::LoopLevel::root().var;
    s = For::make(root_var, 0, 1, For::Serial, LoopSplitInfo(), s);

    for (size_t i = order.size()-1; i > 0; i--) {
        Function f = env.find(order[i-1])->second;

        if (f.schedule().compute_level.is_inline() &&
            !f.schedule().store_level.is_inline()) {
            std::cerr << "Function " << f.name() << " is scheduled to be computed inline, "
                      << "but is not scheduled to be stored inline. A storage schedule "
                      << "makes no sense for functions computed inline" << std::endl;
            assert(false);
        }

        if (!f.is_reduction() && f.schedule().compute_level.is_inline()) {
            log(1) << "Inlining " << order[i-1] << '\n';
            s = InlineFunction(f).mutate(s);
        } else {
            log(1) << "Injecting realization of " << order[i-1] << '\n';
            InjectRealization injector(f);
            s = injector.mutate(s);
            assert(injector.found_store_level && injector.found_compute_level);
        }
        log(2) << s << '\n';
    }

    // We can remove the loop over root now
    const For *root_loop = s.as<For>();
    assert(root_loop);
    return root_loop->body;    
    
}

// Insert checks to make sure a statement doesn't read out of bounds
// on inputs or outputs, and that the inputs and outputs conform to
// the format required (e.g. stride.0 must be 1).
Stmt add_image_checks(Stmt s, Function f) {
    FindBuffers finder;
    s.accept(&finder);
    map<string, FindBuffers::Result> bufs = finder.buffers;    

    bufs[f.name()].type = f.value().type();

    map<string, Region> regions = regions_touched(s);
    for (map<string, FindBuffers::Result>::iterator iter = bufs.begin();
         iter != bufs.end(); ++iter) {
        const string &name = iter->first;
        Buffer &image = iter->second.image;
        Parameter &param = iter->second.param;
        Type type = iter->second.type;

        // Check the elem size matches the internally-understood type
        {
            Expr elem_size = Variable::make(Int(32), name + ".elem_size");
            int correct_size = type.bits / 8;
            ostringstream error_msg;
            error_msg << "Element size for " << name << " should be " << correct_size;
            Stmt check = AssertStmt::make(elem_size == type.bits / 8, error_msg.str()); 
            s = Block::make(check, s);
        }

        // Bounds checking can be disabled via HL_DISABLE_BOUNDS_CHECKING
        const char *disable = getenv("HL_DISABLE_BOUNDS_CHECKING");
        if (!disable || atoi(disable) == 0) {

            // Figure out the region touched
            const Region &region = regions[name];
            if (region.size()) {
                log(3) << "In image " << name << " region touched is:\n";
                for (size_t j = 0; j < region.size(); j++) {
                    log(3) << region[j].min << ", " << region[j].extent << "\n";
                    ostringstream min_name, extent_name;
                    min_name << name << ".min." << j;
                    extent_name << name << ".extent." << j;
                    Expr actual_min = Variable::make(Int(32), min_name.str());
                    Expr actual_extent = Variable::make(Int(32), extent_name.str());
                    Expr min_used = region[j].min;
                    Expr extent_used = region[j].extent;
                    if (!min_used.defined() || !extent_used.defined()) {
                        std::cerr << "Region used of buffer " << name 
                                  << " is unbounded in dimension " << j << std::endl;
                        assert(false);
                    }
                    ostringstream error_msg;
                    error_msg << name << " is accessed out of bounds in dimension " << j;
                    Stmt check = AssertStmt::make((actual_min <= min_used) &&
                                                (actual_min + actual_extent >= min_used + extent_used), 
                                                error_msg.str());
                    s = Block::make(check, s);
                }
            }
        } else {
            log(2) << "Bounds checking disabled via HL_DISABLE_BOUNDS_CHECKING\n";
        }


        // Validate the buffer arguments
        vector<pair<string, Expr> > lets;
        for (int i = 0; i < 4; i++) {
            char dim = '0' + i;
            if (image.defined() && i < image.dimensions()) {
                lets.push_back(make_pair(name + ".stride." + dim, image.stride(i)));
                lets.push_back(make_pair(name + ".extent." + dim, image.extent(i)));
                lets.push_back(make_pair(name + ".min." + dim, image.min(i)));                
            } else if (param.defined()) {
                Expr stride = param.stride_constraint(i);
                Expr extent = param.extent_constraint(i);
                Expr min = param.min_constraint(i);
                if (stride.defined()) {
                    lets.push_back(make_pair(name + ".stride." + dim, stride));
                }
                if (extent.defined()) {
                    lets.push_back(make_pair(name + ".extent." + dim, extent));
                }
                if (min.defined()) {
                    lets.push_back(make_pair(name + ".min." + dim, min));
                }
            }
        }

        // The stride of the output buffer in dimension 0 should also be 1
        lets.push_back(make_pair(f.name() + ".stride.0", 1));

        // Copy the new values to the old names
        for (size_t i = 0; i < lets.size(); i++) {
            s = LetStmt::make(lets[i].first, Variable::make(Int(32), lets[i].first + ".constrained"), s);
        }

        // Assert all the conditions, and set the new values
        vector<Stmt> asserts;
        for (size_t i = 0; i < lets.size(); i++) {
            Expr var = Variable::make(Int(32), lets[i].first);
            Expr value = lets[i].second;
            ostringstream error;
            error << "Static constraint violated: " << lets[i].first << " == " << value;
            asserts.push_back(AssertStmt::make(var == value, error.str()));
            s = LetStmt::make(lets[i].first + ".constrained", value, s);
        }

        for (size_t i = asserts.size(); i > 0; i--) {
            s = Block::make(asserts[i-1], s);
        }

 
    }    

    return s;
}

void compiler_clear() {
    // Clear all shared data objects
    IRLazyScopeBase::clear();
    simplify_clear();
}

Stmt do_bounds_simplify(Stmt s, int section) {
    code_logger.section(section);
	if (global_options.interval_analysis_simplify) {
		//log::debug_level = 1;
		log(1) << "Performing bounds analysis simplification...\n";
		s = simplify(s);
        code_logger.log(s, "simplify");
        code_logger.section("bounds_simplify");
		s = bounds_simplify(s);
		log(2) << "Bounds Simplify:\n" << s << '\n';
		code_logger.log(s, "bounds_simplify");
		//log::debug_level = 0;

		//log(1) << "Simplifying...\n";
		//s = simplify(s);
		//s = remove_dead_lets(s);
		//log(2) << "Simplified: \n" << s << "\n\n";
		//code_logger.log(s, "simplify");
	}
    return s;
}

Stmt do_loop_split(Stmt s, int section) {
    code_logger.section(section);
    if (global_options.loop_split) {
		log(1) << "Simplifying...\n";
		s = simplify(s);
		s = remove_dead_lets(s);
		code_logger.log(s, "deadlets");

		log(1) << "Performing loop split optimization...\n";
		s = loop_split(s);
		log(2) << "Loop split:\n" << s << '\n';
		code_logger.log(s, "loop_split");
	}
	
    return do_bounds_simplify(s, section + 10);
}
    


Stmt lower(Function f) {
    compiler_clear();
    Statistics start_statistics = global_statistics;
    
    propagate_schedule_all(f);
    
    // Compute an environment
    map<string, Function> env;
    populate_environment(f, env);

    // Compute a realization order
    map<string, set<string> > graph;
    vector<string> order = realization_order(f.name(), env, graph);
    Stmt s = create_initial_loop_nest(f);

    code_logger.reset();
    code_logger.section(200);
    code_logger.name(f.name());
    code_logger.log(s, "initial");

    code_logger.section(220);
    s = schedule_functions(s, order, env, graph);
    code_logger.log(s, "realize");
    
    s = simplify(s);
    code_logger.log(s, "simplify");

# if LOWER_CLAMP == 240
    //LH
    // Lowering Clamp here does not produce the same results as using the original clamp.
    code_logger.section(240);
    log(1) << "Lowering Clamp early\n";
    s = lower_clamp(s);
    s = simplify(s);
    code_logger.log(s, "clamp");
# endif

    code_logger.section(250);
    log(1) << "Injecting tracing...\n";
    s = inject_tracing(s);
    log(2) << "Tracing injected:\n" << s << '\n';
    code_logger.log(s, "trace");

    code_logger.section(260);
    log(1) << "Adding checks for images\n";
    s = add_image_checks(s, f);    
    log(2) << "Image checks injected:\n" << s << '\n';

    code_logger.section(270);
    // This pass injects nested definitions of variable names, so we
    // can't simplify from here until we fix them up **** New issue as of 2013 restructure
    log(1) << "Performing bounds inference...\n";
    s = bounds_inference(s, order, env);
    log(2) << "Bounds inference:\n" << s << '\n';
    code_logger.log(s, "bounds");
    
# if LOWER_CLAMP == 290
    //LH
    // Lowering Clamp here does not produce the same results as using the original clamp.
    code_logger.section(290);
    log(1) << "Lowering Clamp late\n";
    s = lower_clamp(s);
    //s = simplify(s);
    log(1) << "Clamp lowered:\n" << s << '\n';
    code_logger.log(s, "clamp");
# endif

    code_logger.section(300);
    log(1) << "Performing sliding window optimization...\n";
    s = sliding_window(s, env);
    log(2) << "Sliding window:\n" << s << '\n';
    code_logger.log(s, "sliding");

    // This uniquifies the variable names, so we're good to simplify
    // after this point. This lets later passes assume syntactic
    // equivalence means semantic equivalence.
    log(1) << "Uniquifying variable names...\n";
    s = uniquify_variable_names(s);
    log(2) << "Uniquified variable names: \n" << s << "\n\n";

# if 0
    // Untried recently.
    s = do_loop_split(s, 320);
#endif

    code_logger.section(340);
    log(1) << "Simplifying...\n";
    s = simplify(s);
    log(2) << "Simplified: \n" << s << "\n\n";
    code_logger.log(s, "simplify");

    code_logger.section(400);
    log(1) << "Performing storage folding optimization...\n";
    s = storage_folding(s);
    log(2) << "Storage folding:\n" << s << '\n';
    code_logger.log(s, "storefold");

    code_logger.section(420);
    log(1) << "Injecting debug_to_file calls...\n";
    s = debug_to_file(s, env);
    log(2) << "Injected debug_to_file calls:\n" << s << '\n';
    code_logger.log(s, "debug_to_file");

    code_logger.section(440);
    log(1) << "Performing storage flattening...\n";
    s = storage_flattening(s, env);
    log(2) << "Storage flattening: " << '\n' << s << "\n\n";
    code_logger.log(s, "flattening");

    log(1) << "Simplifying...\n";
    s = simplify(s);
    log(2) << "Simplified: \n" << s << "\n\n";
    code_logger.log(s, "simplify");

    s = do_loop_split(s, 460);

# if LOWER_CLAMP == 490
    //LH
    code_logger.section(490);
    log(1) << "Lowering Clamp at 490\n";
    s = lower_clamp(s);
    s = simplify(s);
    log(1) << "Clamp lowered:\n" << s << '\n';
    code_logger.log(s, "clamp");
    
    // Strictly, we should run bounds simplify again after lowering clamp.
    // For example, Clamp::Replicate becomes min and max calls.
    // Other clamp expressions could possibly be simplified in certain circumstances.
    // However, when Clamp::Replicate is immediately lowered to clamp(),
    // currently this pass does nothing useful.
    //s = do_bounds_simplify(s, 495);
# endif

    code_logger.section(500);
    log(1) << "Vectorizing...\n";
    s = vectorize_loops(s);
    log(2) << "Vectorized: \n" << s << "\n\n";
    code_logger.log(s, "vector");

    code_logger.section(520);
    log(1) << "Unrolling...\n";
    s = unroll_loops(s);
    log(2) << "Unrolled: \n" << s << "\n\n";
    code_logger.log(s, "unroll");

    log(1) << "Simplifying...\n";
    s = simplify(s);
    log(2) << "Simplified: \n" << s << "\n\n";
    code_logger.log(s, "simplify");

    code_logger.section(540);
    log(1) << "Detecting vector interleavings...\n";
    s = rewrite_interleavings(s);
    log(2) << "Rewrote vector interleavings: \n" << s << "\n\n";    
    code_logger.log(s, "interleave");

    log(1) << "Injecting early frees...\n";
    s = inject_early_frees(s);
    log(2) << "Injected early frees: \n" << s << "\n\n";
    

    //s = do_loop_split(s, 560); // More difficult to work with ramp.
    
    code_logger.section(800);
    log(1) << "Simplifying...\n";
    s = remove_trivial_for_loops(s);
    s = simplify(s);
    log(1) << "Simplified: \n" << s << "\n\n";
    
    code_logger.section(900);
    code_logger.log(s, "final");
    
    Statistics run_statistics = global_statistics;
    run_statistics.Subtract(start_statistics);
    log(1) << run_statistics;

    return s;
} 
   
}
}
