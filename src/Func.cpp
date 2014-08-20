#include "IR.h"
#include "Func.h"
#include "Util.h"
#include "IROperator.h"
#include "IRPrinter.h"
#include "Function.h"
#include "Argument.h"
#include "Lower.h"
#include "StmtCompiler.h"
#include "CodeGen_C.h"
#include "Image.h"
#include "Param.h"
#include "Log.h"
#include "Simplify.h"
#include <iostream>
#include <fstream>

//LH
#include "DomainInference.h"
#include "Interval.h"
#include "CodeLogger.h"

namespace Halide {

using std::max;
using std::min;
using std::make_pair;
using std::string;
using std::vector;
using std::pair;
using std::ofstream;

using namespace Internal;

Func::Func(const string &name) : func(unique_name(name)), 
                                 error_handler(NULL), 
                                 custom_malloc(NULL), 
                                 custom_free(NULL), 
                                 custom_do_par_for(NULL), 
                                 custom_do_task(NULL) {
}

Func::Func() : func(unique_name('f')), 
               error_handler(NULL), 
               custom_malloc(NULL), 
               custom_free(NULL), 
               custom_do_par_for(NULL), 
               custom_do_task(NULL) {
}

Func::Func(Expr e) : func(unique_name('f')),
                     error_handler(NULL), 
                     custom_malloc(NULL), 
                     custom_free(NULL), 
                     custom_do_par_for(NULL), 
                     custom_do_task(NULL) {
    (*this)() = e;
}

/*
Func::Func(Buffer b) : func(unique_name('f')),
                       error_handler(NULL), 
                       custom_malloc(NULL), 
                       custom_free(NULL), 
                       custom_do_par_for(NULL), 
                       custom_do_task(NULL) {
    vector<Expr> args;
    for (int i = 0; i < b.dimensions(); i++) {
        args.push_back(Var::implicit(i));
    }
    (*this)() = Internal::Call::make(b, args);
}
*/
# ifdef IMAGE_TO_FUNC
void Func::constructor(Buffer b) {
    func = Function(unique_name('f'));
    error_handler = NULL;
    custom_malloc = NULL;
    custom_free = NULL;
    custom_do_par_for = NULL;
    custom_do_task = NULL;
    vector<Expr> args;
    for (int i = 0; i < b.dimensions(); i++) {
        args.push_back(Var::implicit(i));
    }
    (*this)() = Internal::Call::make(b, args);
}

Func::Func(Buffer b) {    
    constructor(b);
}
# endif

# ifdef FUNC_EXPLICIT_COPY
void Func::clear() {
    // Retain the name that the programmer assigned to this Func object.
    copy(Func(name()));
}

void Func::copy(Func f) {
    func = f.func;
    lowered = f.lowered;
    compiled_module = f.compiled_module;
    error_handler = f.error_handler;
    custom_malloc = f.custom_malloc;
    custom_free = f.custom_free;
    arg_values = f.arg_values;
    image_param_args = f.image_param_args;
}
# endif
        
const string &Func::name() const {
    return func.name();
}

Expr Func::value() const {
    return func.value();
} 

int Func::dimensions() const {
    if (!func.value().defined()) return 0;
    return (int)func.args().size();
}

FuncRefVar Func::operator()() const {
    // Bulk up the argument list using implicit vars
    vector<Var> args;
    add_implicit_vars(args);
    return FuncRefVar(func, args);
}

FuncRefVar Func::operator()(Var x) const {
    // Bulk up the argument list using implicit vars
    vector<Var> args = vec(x);
    add_implicit_vars(args);
    return FuncRefVar(func, args);
}

FuncRefVar Func::operator()(Var x, Var y) const {
    vector<Var> args = vec(x, y);
    add_implicit_vars(args);
    return FuncRefVar(func, args);
}

FuncRefVar Func::operator()(Var x, Var y, Var z) const{
    vector<Var> args = vec(x, y, z);
    add_implicit_vars(args);
    return FuncRefVar(func, args);
}

FuncRefVar Func::operator()(Var x, Var y, Var z, Var w) const {
    vector<Var> args = vec(x, y, z, w);
    add_implicit_vars(args);
    return FuncRefVar(func, args);
}

FuncRefVar Func::operator()(Var x, Var y, Var z, Var w, Var u) const {
    vector<Var> args = vec(x, y, z, w, u);
    add_implicit_vars(args);
    return FuncRefVar(func, args);
}

FuncRefVar Func::operator()(Var x, Var y, Var z, Var w, Var u, Var v) const {
  vector<Var> args = vec(x, y, z, w, u, v);
    add_implicit_vars(args);
    return FuncRefVar(func, args);
}

FuncRefVar Func::operator()(vector<Var> args) const {
    add_implicit_vars(args);
    return FuncRefVar(func, args);
}
 
FuncRefExpr Func::operator()(Expr x) const {
    vector<Expr> args = vec(x);
    add_implicit_vars(args);
    return FuncRefExpr(func, args);
}

FuncRefExpr Func::operator()(Expr x, Expr y) const {
    vector<Expr> args = vec(x, y);
    add_implicit_vars(args);
    return FuncRefExpr(func, args);
}

FuncRefExpr Func::operator()(Expr x, Expr y, Expr z) const {
    vector<Expr> args = vec(x, y, z);
    add_implicit_vars(args);
    return FuncRefExpr(func, args);
}

FuncRefExpr Func::operator()(Expr x, Expr y, Expr z, Expr w) const {
    vector<Expr> args = vec(x, y, z, w);
    add_implicit_vars(args);
    return FuncRefExpr(func, args);
}  

FuncRefExpr Func::operator()(Expr x, Expr y, Expr z, Expr w, Expr u) const {
  vector<Expr> args = vec(x, y, z, w, u);
    add_implicit_vars(args);
    return FuncRefExpr(func, args);
}  

FuncRefExpr Func::operator()(Expr x, Expr y, Expr z, Expr w, Expr u, Expr v) const {
    vector<Expr> args = vec(x, y, z, w, u, v);
    add_implicit_vars(args);
    return FuncRefExpr(func, args);
}  

FuncRefExpr Func::operator()(vector<Expr> args) const {
    add_implicit_vars(args);
    return FuncRefExpr(func, args);
}

FuncRefKernel Func::operator[](Expr x) {
    return FuncRefKernel(func, vec(x));
}

FuncRefKernel Func::operator[](std::vector<Expr> v) {
    return FuncRefKernel(func, v);
}

FuncRefKernel FuncRefKernel::operator[](Expr x) {
    // Accumulate indices in C style [1][2][3]...
    args.push_back(x);
    return (*this);
}

FuncRefKernel::operator FuncRefExpr() const {
    vector<Expr> imp_args;
    for (int i = 0; i < (int) func.args().size(); i++) {
        if (i < (int) args.size()) {
            // Ensure that the kernel argument expression is just an integer constant
            // after folding.  Programmer can write Expr(1) + Expr(3) if they really want to...
            Expr e = simplify(args[i]);
            if (! is_const(e)) {
                std::cerr << "Kernel expression [" << args[i] << "] is not allowed. Kernel expressions must be constant expressions\n";
                assert(0 && "Invalid kernel expression");
            }
            if (e.type() != Int(32)) {
                e = cast(Int(32), e); // Force to Int(32) so that we dont end up with a floating point index expression
            }
            imp_args.push_back(Var::implicit(i) + e);
        } else {
            imp_args.push_back(Var::implicit(i));
        }
    }
    return FuncRefExpr(func, imp_args);
}

void Func::add_implicit_vars(vector<Var> &args) const {
    int i = 0;    
    while ((int)args.size() < dimensions()) {        
        Internal::log(2) << "Adding implicit var " << i << " to call to " << name() << "\n";
        args.push_back(Var::implicit(i++));
    }
}
    
void Func::add_implicit_vars(vector<Expr> &args) const {
    int i = 0;
    while ((int)args.size() < dimensions()) {
        Internal::log(2) << "Adding implicit var " << i << " to call to " << name() << "\n";
        args.push_back(Var::implicit(i++));
    }
}


namespace {
bool var_name_match(string candidate, string var) {
    if (candidate == var) return true;
    return Internal::ends_with(candidate, "." + var);
}
}

void ScheduleHandle::set_dim_type(Var var, For::ForType t) {
    bool found = false;
    vector<Schedule::Dim> &dims = schedule.dims;
    for (size_t i = 0; i < dims.size(); i++) {
        if (var_name_match(dims[i].var, var.name())) {
            found = true;
            dims[i].for_type = t;
        } else if (t == For::Vectorized) {
            assert(dims[i].for_type != For::Vectorized && 
                   "Can't vectorize across more than one variable");
        }
    }
        
    if (! found) // LH provide more information if cannot find dimensions
    {
        Internal::log(0) << "Searched for " << var.name() << " in";
        for (size_t i = 0; (!found) && i < dims.size(); i++) {
            Internal::log(0) << " " << dims[i].var;
        }
        Internal::log(0) << "\n";
    }
    if (!found) {
        std::cerr << "Could not find dimension " 
                  << var.name() 
                  << " to mark as " << t
                  << " in argument list for function\n";
        dump_argument_list();
        assert(false);
    }
}

void ScheduleHandle::dump_argument_list() {
    std::cerr << "Argument list:";
    for (size_t i = 0; i < schedule.dims.size(); i++) {
        std::cerr << " " << schedule.dims[i].var;
    }
    std::cerr << "\n";
}

ScheduleHandle &ScheduleHandle::split(Var old, Var outer, Var inner, Expr factor) {
    // Replace the old dimension with the new dimensions in the dims list
    bool found = false;
    string inner_name, outer_name, old_name;
    vector<Schedule::Dim> &dims = schedule.dims;
    for (size_t i = 0; (!found) && i < dims.size(); i++) {
        if (var_name_match(dims[i].var, old.name())) {
            found = true;
            old_name = dims[i].var;
            inner_name = old_name + "." + inner.name();
            outer_name = old_name + "." + outer.name();
            dims[i].var = inner_name;
            //dims.push_back(dims[dims.size()-1]);
            dims.push_back(Schedule::Dim()); //LH Push back an empty Dim object
            for (size_t j = dims.size(); j > i+1; j--) {
                dims[j-1] = dims[j-2];
            }
            dims[i+1].var = outer_name;
            dims[i+1].for_type = dims[i].for_type;
            // If loop splitting was applied to the unsplit variable, it must now
            // be applied to the outer variable, not the inner variable.
            // The main loop bounds must now be unzoomed by the variable split factor
            // to derive bounds on the outer variable.
            dims[i+1].loop_split.interval = unzoom(dims[i+1].loop_split.interval, factor);
            dims[i].loop_split = LoopSplitInfo(false); // Do not split the inner loop
            
        }
    }
        
    if (!found) {
        std::cerr << "Could not find split dimension in argument list: " 
                  << old.name() 
                  << "\n";
        dump_argument_list();
        assert(false);
    }

        
    // Add the split to the splits list
    Schedule::Split split = {old_name, outer_name, inner_name, factor, false};
    schedule.splits.push_back(split);
    return *this;
}

ScheduleHandle &ScheduleHandle::rename(Var old_var, Var new_var) {
    // Replace the old dimension with the new dimensions in the dims list
    bool found = false;
    vector<Schedule::Dim> &dims = schedule.dims;
    for (size_t i = 0; (!found) && i < dims.size(); i++) {
        if (var_name_match(dims[i].var, old_var.name())) {
            found = true;
            dims[i].var += "." + new_var.name();
        }
    }
     
    if (!found) {
        std::cerr << "Could not find rename dimension in argument list: " 
                  << old_var.name() 
                  << "\n";
        dump_argument_list();
        assert(false);
    }
        
    // Add the rename to the splits list
    Schedule::Split split = {old_var.name(), old_var.name() + "." + new_var.name(), "", 1, true};
    schedule.splits.push_back(split);
    return *this;
}

ScheduleHandle &ScheduleHandle::parallel(Var var) {
    set_dim_type(var, For::Parallel);
    return *this;
}

ScheduleHandle &ScheduleHandle::vectorize(Var var) {
    set_dim_type(var, For::Vectorized);
    return *this;
}

ScheduleHandle &ScheduleHandle::unroll(Var var) {
    set_dim_type(var, For::Unrolled);
    return *this;
}

ScheduleHandle &ScheduleHandle::vectorize(Var var, int factor) {
    Var tmp;
    split(var, var, tmp, factor);
    vectorize(tmp);
    return *this;
}

ScheduleHandle &ScheduleHandle::unroll(Var var, int factor) {
    Var tmp;
    split(var, var, tmp, factor);
    unroll(tmp);
    return *this;
}

ScheduleHandle &ScheduleHandle::bound(Var var, Expr min, Expr extent) {
    Schedule::Bound b = {var.name(), min, extent};
    schedule.bounds.push_back(b);
    return *this;
}

ScheduleHandle &ScheduleHandle::tile(Var x, Var y, Var xo, Var yo, Var xi, Var yi, Expr xfactor, Expr yfactor) {
    split(x, xo, xi, xfactor);
    split(y, yo, yi, yfactor);
    reorder(xi, yi, xo, yo);
    return *this;
}

ScheduleHandle &ScheduleHandle::tile(Var x, Var y, Var xi, Var yi, Expr xfactor, Expr yfactor) {
    split(x, x, xi, xfactor);
    split(y, y, yi, yfactor);
    reorder(xi, yi, x, y);
    return *this;
}

ScheduleHandle &ScheduleHandle::reorder(Var x, Var y) {
    vector<Schedule::Dim> &dims = schedule.dims;
    bool found_y = false;
    size_t y_loc = 0;
    for (size_t i = 0; i < dims.size(); i++) {
        if (var_name_match(dims[i].var, y.name())) {
            found_y = true;
            y_loc = i;
        } else if (var_name_match(dims[i].var, x.name())) {
            if (found_y) std::swap(dims[i], dims[y_loc]);
            return *this;
        }
    }
    assert(false && "Could not find these variables to reorder in schedule");
    return *this;
}
    

ScheduleHandle &ScheduleHandle::reorder(Var x, Var y, Var z) {
    return reorder(x, y).reorder(x, z).reorder(y, z);
}

ScheduleHandle &ScheduleHandle::reorder(Var x, Var y, Var z, Var w) {
    return reorder(x, y).reorder(x, z).reorder(x, w).reorder(y, z, w);
}

ScheduleHandle &ScheduleHandle::reorder(Var x, Var y, Var z, Var w, Var t) {
    return reorder(x, y).reorder(x, z).reorder(x, w).reorder(x, t).reorder(y, z, w, t);
}

ScheduleHandle &ScheduleHandle::cuda_threads(Var tx) {
    parallel(tx);
    rename(tx, Var("threadidx"));
    return *this;
}
 
namespace {
void record_loop_split(Internal::Schedule &schedule, Var var, LoopSplitInfo info) {
    bool found = false;
    vector<Schedule::Dim> &dims = schedule.dims;
    for (size_t i = 0; (!found) && i < dims.size(); i++) {
        if (var_name_match(dims[i].var, var.name())) {
            found = true;
            dims[i].loop_split = info;
        }
    }
    
    if (! found) // LH provide more information if cannot find dimensions
    {
        Internal::log(0) << "Searched for " << var.name() << " in";
        for (size_t i = 0; (!found) && i < dims.size(); i++) {
            Internal::log(0) << " " << dims[i].var;
        }
        Internal::log(0) << "\n";
    }
    assert(found && "Could not find dimension in argument list for function");
    return;
}
}
 
ScheduleHandle &ScheduleHandle::loop_split(Var var, bool auto_split) {
    record_loop_split(schedule, var, LoopSplitInfo(auto_split));
    return *this;
}
 
ScheduleHandle &ScheduleHandle::loop_split(Var var, DomInterval interval) {
    record_loop_split(schedule, var, LoopSplitInfo(interval));
    return *this;
}

ScheduleHandle &ScheduleHandle::loop_split(bool auto_split) {
    schedule.loop_split_settings.auto_split = auto_split ? LoopSplitInfo::Yes : LoopSplitInfo::No;
    return *this;
}

ScheduleHandle &ScheduleHandle::loop_split_all(bool auto_split) {
    schedule.loop_split_settings.auto_split_all = auto_split ? LoopSplitInfo::Yes : LoopSplitInfo::No;
    return *this;
}

ScheduleHandle &ScheduleHandle::loop_split_borders(bool split_borders) {
    schedule.loop_split_settings.split_borders = split_borders ? LoopSplitInfo::Yes : LoopSplitInfo::No;
    return *this;
}

ScheduleHandle &ScheduleHandle::loop_split_borders_all(bool split_borders) {
    schedule.loop_split_settings.split_borders_all = split_borders ? LoopSplitInfo::Yes : LoopSplitInfo::No;
    return *this;
}


ScheduleHandle &ScheduleHandle::cuda_threads(Var tx, Var ty) {
    parallel(tx);
    parallel(ty);
    rename(tx, Var("threadidx"));
    rename(ty, Var("threadidy"));
    return *this;
}

ScheduleHandle &ScheduleHandle::cuda_threads(Var tx, Var ty, Var tz) {
    parallel(tx);
    parallel(ty);
    parallel(tz);
    rename(tx, Var("threadidx"));
    rename(ty, Var("threadidy"));
    rename(tz, Var("threadidz"));
    return *this;
}

ScheduleHandle &ScheduleHandle::cuda_blocks(Var tx) {
    parallel(tx);
    rename(tx, Var("blockidx"));
    return *this;
}

ScheduleHandle &ScheduleHandle::cuda_blocks(Var tx, Var ty) {
    parallel(tx);
    parallel(ty);
    rename(tx, Var("blockidx"));
    rename(ty, Var("blockidy"));
    return *this;
}

ScheduleHandle &ScheduleHandle::cuda_blocks(Var tx, Var ty, Var tz) {
    parallel(tx);
    parallel(ty);
    parallel(tz);
    rename(tx, Var("blockidx"));
    rename(ty, Var("blockidy"));
    rename(tz, Var("blockidz"));
    return *this;
}

ScheduleHandle &ScheduleHandle::cuda(Var bx, Var tx) {
    return cuda_blocks(bx).cuda_threads(tx);
}

ScheduleHandle &ScheduleHandle::cuda(Var bx, Var by, 
                                     Var tx, Var ty) {
    return cuda_blocks(bx, by).cuda_threads(tx, ty);
}

ScheduleHandle &ScheduleHandle::cuda(Var bx, Var by, Var bz, 
                                     Var tx, Var ty, Var tz) {
    return cuda_blocks(bx, by, bz).cuda_threads(tx, ty, tz);
}

ScheduleHandle &ScheduleHandle::cuda_tile(Var x, int x_size) {
    Var bx("blockidx"), tx("threadidx");
    split(x, bx, tx, x_size);
    parallel(bx);
    parallel(tx);
    return *this;
}


ScheduleHandle &ScheduleHandle::cuda_tile(Var x, Var y, 
                                          int x_size, int y_size) {
    Var bx("blockidx"), by("blockidy"), tx("threadidx"), ty("threadidy");
    tile(x, y, bx, by, tx, ty, x_size, y_size);
    parallel(bx);
    parallel(by);
    parallel(tx);
    parallel(ty);
    return *this;
}

ScheduleHandle &ScheduleHandle::cuda_tile(Var x, Var y, Var z, 
                                          int x_size, int y_size, int z_size) {
    Var bx("blockidx"), by("blockidy"), bz("blockidz"),
        tx("threadidx"), ty("threadidy"), tz("threadidz");
    split(x, bx, tx, x_size);
    split(y, by, ty, y_size);
    split(z, bz, tz, z_size);
    // current order is:
    // tx bx ty by tz bz
    reorder(ty, bx);
    // tx ty bx by tz bz
    reorder(tz, bx);
    // tx ty tz by bx bz
    reorder(bx, by);
    // tx ty tz bx by bz
    parallel(bx);
    parallel(by);
    parallel(bz);
    parallel(tx);
    parallel(ty);
    parallel(tz);
    return *this;
}

Func &Func::split(Var old, Var outer, Var inner, Expr factor) {
    ScheduleHandle(func.schedule()).split(old, outer, inner, factor);
    return *this;
}

Func &Func::rename(Var old_name, Var new_name) {
    ScheduleHandle(func.schedule()).rename(old_name, new_name);
    return *this;
}

Func &Func::parallel(Var var) {
    ScheduleHandle(func.schedule()).parallel(var);
    return *this;
}

Func &Func::vectorize(Var var) {
    ScheduleHandle(func.schedule()).vectorize(var);
    return *this;
}

Func &Func::unroll(Var var) {
    ScheduleHandle(func.schedule()).unroll(var);
    return *this;
}

Func &Func::vectorize(Var var, int factor) {
    ScheduleHandle(func.schedule()).vectorize(var, factor);
    return *this;
}

Func &Func::unroll(Var var, int factor) {
    ScheduleHandle(func.schedule()).unroll(var, factor);
    return *this;
}

Func &Func::bound(Var var, Expr min, Expr extent) {
    ScheduleHandle(func.schedule()).bound(var, min, extent);
    return *this;
}

Func &Func::tile(Var x, Var y, Var xo, Var yo, Var xi, Var yi, Expr xfactor, Expr yfactor) {
    ScheduleHandle(func.schedule()).tile(x, y, xo, yo, xi, yi, xfactor, yfactor);
    return *this;
}

Func &Func::tile(Var x, Var y, Var xi, Var yi, Expr xfactor, Expr yfactor) {
    ScheduleHandle(func.schedule()).tile(x, y, xi, yi, xfactor, yfactor);
    return *this;
}

Func &Func::reorder(Var x, Var y) {
    ScheduleHandle(func.schedule()).reorder(x, y);
    return *this;
}    

Func &Func::reorder(Var x, Var y, Var z) {
    ScheduleHandle(func.schedule()).reorder(x, y, z);
    return *this;
}

Func &Func::reorder(Var x, Var y, Var z, Var w) {
    ScheduleHandle(func.schedule()).reorder(x, y, z, w);
    return *this;
}

Func &Func::reorder(Var x, Var y, Var z, Var w, Var t) {
    ScheduleHandle(func.schedule()).reorder(x, y, z, w, t);
    return *this;
}

Func &Func::cuda_threads(Var tx) {
    ScheduleHandle(func.schedule()).cuda_threads(tx);
    return *this;
}

Func &Func::cuda_threads(Var tx, Var ty) {
    ScheduleHandle(func.schedule()).cuda_threads(tx, ty);
    return *this;
}

Func &Func::cuda_threads(Var tx, Var ty, Var tz) {
    ScheduleHandle(func.schedule()).cuda_threads(tx, ty, tz);
    return *this;
}

Func &Func::cuda_blocks(Var bx) {
    ScheduleHandle(func.schedule()).cuda_blocks(bx);
    return *this;
}

Func &Func::cuda_blocks(Var bx, Var by) {
    ScheduleHandle(func.schedule()).cuda_blocks(bx, by);
    return *this;
}

Func &Func::cuda_blocks(Var bx, Var by, Var bz) {
    ScheduleHandle(func.schedule()).cuda_blocks(bx, by, bz);
    return *this;
}

Func &Func::cuda(Var bx, Var tx) {
    ScheduleHandle(func.schedule()).cuda(bx, tx);
    return *this;
}

Func &Func::cuda(Var bx, Var by, Var tx, Var ty) {
    ScheduleHandle(func.schedule()).cuda(bx, by, tx, ty);
    return *this;
}

Func &Func::cuda(Var bx, Var by, Var bz, Var tx, Var ty, Var tz) {
    ScheduleHandle(func.schedule()).cuda(bx, by, bz, tx, ty, tz);
    return *this;
}

Func &Func::cuda_tile(Var x, int x_size) {
    ScheduleHandle(func.schedule()).cuda_tile(x, x_size);
    return *this;
}

Func &Func::cuda_tile(Var x, Var y, int x_size, int y_size) {
    ScheduleHandle(func.schedule()).cuda_tile(x, y, x_size, y_size);
    return *this;
}

Func &Func::cuda_tile(Var x, Var y, Var z, int x_size, int y_size, int z_size) {
    ScheduleHandle(func.schedule()).cuda_tile(x, y, z, x_size, y_size, z_size);
    return *this;
}


// LH
Func &Func::loop_split(Var x, DomInterval split) {
    ScheduleHandle(func.schedule()).loop_split(x, split);
    return *this;
}

Func &Func::loop_split(Var x, bool auto_split) {
    ScheduleHandle(func.schedule()).loop_split(x, auto_split);
    return *this;
}

Func &Func::loop_split(bool auto_split) {
    ScheduleHandle(func.schedule()).loop_split(auto_split);
    update().loop_split(auto_split);
    return *this;
}

Func &Func::loop_split_all(bool auto_split) {
    ScheduleHandle(func.schedule()).loop_split_all(auto_split);
    update().loop_split_all(auto_split);
    return *this;
}

Func &Func::loop_split_borders(bool split_borders) {
    ScheduleHandle(func.schedule()).loop_split_borders(split_borders);
    update().loop_split_borders(split_borders);
    return *this;
}

Func &Func::loop_split_borders_all(bool split_borders) {
    ScheduleHandle(func.schedule()).loop_split_borders_all(split_borders);
    update().loop_split_borders(split_borders);
    return *this;
}



Func &Func::reorder_storage(Var x, Var y) {
    vector<string> &dims = func.schedule().storage_dims;
    bool found_y = false;
    size_t y_loc = 0;
    for (size_t i = 0; i < dims.size(); i++) {
        if (var_name_match(dims[i], y.name())) {
            found_y = true;
            y_loc = i;
        } else if (var_name_match(dims[i], x.name())) {
            if (found_y) std::swap(dims[i], dims[y_loc]);
            return *this;
        }
    }
    assert(false && "Could not find these variables to reorder in schedule");
    return *this;    
}

Func &Func::reorder_storage(Var x, Var y, Var z) {
    reorder_storage(x, y);
    reorder_storage(x, z);
    reorder_storage(y, z);
    return *this;
}

Func &Func::reorder_storage(Var x, Var y, Var z, Var w) {
    reorder_storage(x, y);
    reorder_storage(x, z);
    reorder_storage(x, w);
    reorder_storage(y, z, w);
    return *this;
}

Func &Func::reorder_storage(Var x, Var y, Var z, Var w, Var t) {
    reorder_storage(x, y);
    reorder_storage(x, z);
    reorder_storage(x, w);
    reorder_storage(x, t);
    reorder_storage(y, z, w, t);
    return *this;
}

Func &Func::compute_at(Func f, RVar var) {
    return compute_at(f, Var(var.name()));
}

Func &Func::compute_at(Func f, Var var) {
    Schedule::LoopLevel loop_level(f.name(), var.name());
    func.schedule().compute_level = loop_level;
    if (func.schedule().store_level.is_inline()) {
        func.schedule().store_level = loop_level;
    }
    return *this;
}
        
Func &Func::compute_root() {
    func.schedule().compute_level = Schedule::LoopLevel::root();
    func.schedule().store_level = Schedule::LoopLevel::root();
    return *this;
}

Func &Func::store_at(Func f, RVar var) {
    return store_at(f, Var(var.name()));
}

Func &Func::store_at(Func f, Var var) {
    func.schedule().store_level = Schedule::LoopLevel(f.name(), var.name());
    return *this;
}

Func &Func::store_root() {
    func.schedule().store_level = Schedule::LoopLevel::root();
    return *this;
}

Func &Func::compute_inline() {
    func.schedule().compute_level = Schedule::LoopLevel();
    func.schedule().store_level = Schedule::LoopLevel();
    return *this;
}

void Func::debug_to_file(const string &filename) {
    func.debug_file() = filename;    
}

ScheduleHandle Func::update() {
    return ScheduleHandle(func.reduction_schedule());
}

FuncRefVar::FuncRefVar(Internal::Function f, const vector<Var> &a) : func(f) {
    args.resize(a.size());
    for (size_t i = 0; i < a.size(); i++) {
        args[i] = a[i].name();
    }
}           
    
//LH
/** Get a handle to the valid domain for the purpose of modifying it */
Domain &Func::set_domain(Domain::DomainType dt) {
    return func.set_domain(dt);
}

//LH
/** Get a handle to the valid domain for the purpose of inspecting it */
const Domain &Func::domain(Domain::DomainType dt) const {
    return func.domain(dt);
}

//LH
/** Set the valid domain in a schedule format */
Func &Func::domain(Domain::DomainType dt, Domain d) {
    func.set_domain(dt) = d;
    return *this;
}

//LH
/** Set the valid domain to be the same as an existing Func in a schedule format */
Func &Func::domain(Domain::DomainType dt, Func f) {
    func.set_domain(dt) = f.domain(dt);
    return *this;
}

//LH
/** Methods to indicate that the current function is a local operator of other functions. */
Func &Func::local(Func f1) {
    Internal::log(0) << name() << ".local(" << f1.name() << ")\n";
    set_valid() = computable().intersection(f1.valid());
    return *this;
}
Func &Func::local(Func f1, Func f2) {
    Internal::log(0) << name() << ".local(" << f1.name() << ", " << f2.name() << ")\n";
    set_valid() = computable().intersection(f1.valid()).intersection(f2.valid());
    return *this;
}
Func &Func::local(Func f1, Func f2, Func f3) {
    set_valid() = computable().intersection(f1.valid()).intersection(f2.valid()).intersection(f3.valid());
    return *this;
}
Func &Func::local(Func f1, Func f2, Func f3, Func f4) {
    set_valid() = computable().intersection(f1.valid()).intersection(f2.valid()).intersection(f3.valid())
                              .intersection(f4.valid());
    return *this;
}
Func &Func::local(Func f1, Func f2, Func f3, Func f4, Func f5) {
    set_valid() = computable().intersection(f1.valid()).intersection(f2.valid()).intersection(f3.valid())
                              .intersection(f4.valid()).intersection(f5.valid());
    return *this;
}
Func &Func::local(Func f1, Func f2, Func f3, Func f4, Func f5, Func f6) {
    set_valid() = computable().intersection(f1.valid()).intersection(f2.valid()).intersection(f3.valid())
                              .intersection(f4.valid()).intersection(f5.valid()).intersection(f6.valid());
    return *this;
}

namespace {
class CountImplicitVars : public Internal::IRVisitor {
public:
    int count;
    CountImplicitVars(Expr e) : count(0) {
        e.accept(this);
    }

    using IRVisitor::visit;

    void visit(const Variable *v) {
        if (v->name.size() > 3 && v->name.substr(0, 3) == "iv.") {
            int n = atoi(v->name.c_str()+3);
            if (n >= count) count = n+1;
        }
    }    
};
}

void FuncRefVar::add_implicit_vars(vector<string> &a, Expr e) const {
    CountImplicitVars count(e);
    Internal::log(2) << "Adding " << count.count << " implicit vars to LHS of " << func.name() << "\n";
    for (int i = 0; i < count.count; i++) {
        a.push_back(Var::implicit(i).name());
    }    
}

void FuncRefVar::operator=(Expr e) {            
    // If the function has already been defined, this must actually be a reduction
    if (func.value().defined()) {
        FuncRefExpr(func, args) = e;
        return;
    }

    // Find implicit args in the expr and add them to the args list before calling define
    vector<string> a = args;
    add_implicit_vars(a, e);
    
    code_logger.reset();
    code_logger.name(func.name());
    code_logger.section(100, "definition");
    //code_logger.log() << func.name();
    code_logger.log() << "(";
    if (a.size() > 0) code_logger.log() << a[0];
    for (size_t i = 1; i < a.size(); i++) code_logger.log() << ", " << a[i];
    code_logger.log() << ") = \n";
    code_logger.log(e);

    func.define(a, e);
}
    
void FuncRefVar::operator+=(Expr e) {
    // This is actually a reduction
    FuncRefExpr(func, args) += e;
}

void FuncRefVar::operator*=(Expr e) {
    // This is actually a reduction
    FuncRefExpr(func, args) *= e;
}

void FuncRefVar::operator-=(Expr e) {
    // This is actually a reduction
    FuncRefExpr(func, args) -= e;
}

void FuncRefVar::operator/=(Expr e) {
    // This is actually a reduction
    FuncRefExpr(func, args) /= e;
}

FuncRefVar::operator Expr() const {
    if (! func.value().defined()) { //LH
        std::cerr << "Attempt to call undefined function " << func.name() << "\n";
        assert(func.value().defined() && "Can't call function with undefined value");
    }
    vector<Expr> expr_args(args.size());
    for (size_t i = 0; i < expr_args.size(); i++) {
        expr_args[i] = Var(args[i]);
    }
    return Call::make(func, expr_args);
}

FuncRefExpr::FuncRefExpr(Internal::Function f, const vector<Expr> &a) : func(f), args(a) {
    for (size_t i = 0; i < args.size(); i++) {
        args[i] = cast<int>(args[i]);
    }
}

FuncRefExpr::FuncRefExpr(Internal::Function f, const vector<string> &a) : func(f) {
    args.resize(a.size());
    for (size_t i = 0; i < a.size(); i++) {
        args[i] = Var(a[i]);
    }
}
    
FuncRefKernel::FuncRefKernel(Internal::Function f, const vector<Expr> &a) : func(f), args(a) {
    for (size_t i = 0; i < args.size(); i++) {
        args[i] = cast<int>(args[i]);
    }
}

void FuncRefExpr::add_implicit_vars(vector<Expr> &a, Expr e) const {
    CountImplicitVars f(e);
    // Implicit vars are also allowed in the lhs of a reduction. E.g.:
    // f(x, y) = x+y
    // g(x, y) = 0
    // g(f(r.x)) = 1   (this means g(f(r.x, i0), i0) = 1)

    for (size_t i = 0; i < args.size(); i++) {
        args[i].accept(&f);
    }
    Internal::log(2) << "Adding " << f.count << " implicit vars to LHS of " << func.name() << "\n";
    for (int i = 0; i < f.count; i++) {
        a.push_back(Var::implicit(i));
    }
}

void FuncRefExpr::operator=(Expr e) {
    assert(func.value().defined() && 
           "Can't add a reduction definition to an undefined function");
    
    vector<Expr> a = args;
    add_implicit_vars(a, e);

    code_logger.reset();
    code_logger.name(func.name());
    code_logger.section(150, "reduction");
    code_logger.log() << func.name() << "(" << a << ") = \n";
    code_logger.log(e);

    func.define_reduction(args, e);
}

// Inject a suitable base-case definition given a reduction
// definition. This is a helper for FuncRefExpr::operator+= and co.
void define_base_case(Internal::Function func, const vector<Expr> &a, Expr e) {
    if (func.value().defined()) return;
    vector<Var> pure_args(a.size());

    // Reuse names of existing pure args
    for (size_t i = 0; i < a.size(); i++) {
        if (const Variable *v = a[i].as<Variable>()) {
            if (!v->param.defined()) pure_args[i] = Var(v->name);
        }
    }    

    FuncRefVar(func, pure_args) = e;
}

void FuncRefExpr::operator+=(Expr e) {
    vector<Expr> a = args;
    add_implicit_vars(a, e);
    define_base_case(func, a, cast(e.type(), 0));
    (*this) = Expr(*this) + e;
}

void FuncRefExpr::operator*=(Expr e) {
    vector<Expr> a = args;
    add_implicit_vars(a, e);
    define_base_case(func, a, cast(e.type(), 1));
    (*this) = Expr(*this) * e;
}

void FuncRefExpr::operator-=(Expr e) {
    vector<Expr> a = args;
    add_implicit_vars(a, e);
    define_base_case(func, a, cast(e.type(), 0));
    (*this) = Expr(*this) - e;
}

void FuncRefExpr::operator/=(Expr e) {
    vector<Expr> a = args;
    add_implicit_vars(a, e);
    define_base_case(func, a, cast(e.type(), 1));
    (*this) = Expr(*this) / e;
}

FuncRefExpr::operator Expr() const {
    if (! func.value().defined()) {
        std::cout << "Error: Can't call function with undefined value: " << func.name() << "\n";
        assert(0 && "Abort");
    }
    return Call::make(func, args);
}

Buffer Func::realize(int x_size, int y_size, int z_size, int w_size) {
    assert(value().defined() && "Can't realize undefined function");
    Type t = value().type();
    Buffer buf(t, x_size, y_size, z_size, w_size);
    realize(buf);
    return buf;
}

namespace {

class InferArguments : public IRVisitor {
public:
    vector<Argument> arg_types;
    vector<const void *> arg_values;
    vector<pair<int, Internal::Parameter> > image_param_args;    

private:
    using IRVisitor::visit;

    void visit(const Load *op) {
        IRVisitor::visit(op);

        Buffer b;
        string arg_name;
        if (op->image.defined()) {            
            Internal::log(2) << "Found an image argument: " << op->image.name() << "\n";
            b = op->image;
            arg_name = op->image.name();
        } else if (op->param.defined()) {
            Internal::log(2) << "Found an image param argument: " << op->param.name() << "\n";
            b = op->param.get_buffer();
            arg_name = op->param.name();
        } else {
            return;
        }

        Argument arg(arg_name, true, op->type);
        bool already_included = false;
        for (size_t i = 0; i < arg_types.size(); i++) {
            if (arg_types[i].name == op->name) {
                Internal::log(2) << "Already included.\n";
                already_included = true;
            }
        }
        if (!already_included) {
            if (op->param.defined()) {
                int idx = (int)(arg_values.size());
                image_param_args.push_back(make_pair(idx, op->param));
            }
            arg_types.push_back(arg);
            if (op->image.defined()) {
                assert(b.defined());
                arg_values.push_back(b.raw_buffer());
            } else {
                arg_values.push_back(NULL);
            }
        }
    }

    void visit(const Variable *op) {
        if (op->param.defined()) {
            Argument arg(op->param.name(), op->param.is_buffer(), op->param.type());
            bool already_included = false;
            for (size_t i = 0; i < arg_types.size(); i++) {
                if (arg_types[i].name == op->param.name()) {
                    already_included = true;
                }
            }
            if (!already_included) {
                Internal::log(2) << "Found a param: " << op->param.name() << "\n";
                if (op->param.is_buffer()) {
                    int idx = (int)(arg_values.size());
                    image_param_args.push_back(make_pair(idx, op->param));                    
                    arg_values.push_back(NULL);
                } else {
                    arg_values.push_back(op->param.get_scalar_address());
                }
                arg_types.push_back(arg);

            }            
        }
    }
};

/** Check that all the necessary arguments are in an args vector */
void validate_arguments(const vector<Argument> &args, Stmt lowered) {
    InferArguments infer_args;
    lowered.accept(&infer_args);
    const vector<Argument> &required_args = infer_args.arg_types;

    for (size_t i = 0; i < required_args.size(); i++) {
        const Argument &arg = required_args[i];
        bool found = false;
        for (size_t j = 0; !found && j < args.size(); j++) {
            if (args[j].name == arg.name) {
                found = true;
            }
        }
        if (!found) {
            std::cerr << "Generated code refers to ";
            if (arg.is_buffer) std::cerr << "image ";
            std::cerr << "parameter " << arg.name 
                      << ", which was not found in the argument list\n";

            std::cerr << "\nArgument list specified: ";
            for (size_t i = 0; i < args.size(); i++) {
                std::cerr << args[i].name << " ";
            }
            std::cerr << "\n\nParameters referenced in generated code: ";
            for (size_t i = 0; i < required_args.size(); i++) {
                std::cerr << required_args[i].name << " ";
            }
            std::cerr << "\n\n";
            assert(false);
        }
    }
}
};


# if 0
Buffer Func::realize() {
    assert(value().defined() && "Can't realize undefined function");
    Type t = value().type();
    Buffer buf(t, x_size, y_size, z_size, w_size);
    realize(buf);
    return buf;
}
#endif

Internal::Stmt Func::compile_to_stmt() {
    assert(value().defined() && "Can't compile undefined function");    

    if (!lowered.defined()) lowered = Halide::Internal::lower(func);
    return lowered;
}


void Func::compile_to_bitcode(const string &filename, vector<Argument> args, const string &fn_name) {
    assert(value().defined() && "Can't compile undefined function");    

    if (!lowered.defined()) {
        lowered = Halide::Internal::lower(func);
    }

    validate_arguments(args, lowered);

    Argument me(name(), true, value().type());
    args.push_back(me);

    StmtCompiler cg;
    cg.compile(lowered, fn_name.empty() ? name() : fn_name, args);
    cg.compile_to_bitcode(filename);
}

void Func::compile_to_object(const string &filename, vector<Argument> args, const string &fn_name) {
    assert(value().defined() && "Can't compile undefined function");    

    if (!lowered.defined()) {
        lowered = Halide::Internal::lower(func);
    }

    validate_arguments(args, lowered);

    Argument me(name(), true, value().type());
    args.push_back(me);

    StmtCompiler cg;
    cg.compile(lowered, fn_name.empty() ? name() : fn_name, args);
    cg.compile_to_native(filename, false);
}

void Func::compile_to_header(const string &filename, vector<Argument> args, const string &fn_name) {    
    Argument me(name(), true, value().type());
    args.push_back(me);

    ofstream header(filename.c_str());
    CodeGen_C cg(header);
    cg.compile_header(fn_name.empty() ? name() : fn_name, args);
}

void Func::compile_to_c(const string &filename, vector<Argument> args, const string &fn_name) {    
    if (!lowered.defined()) {
        lowered = Halide::Internal::lower(func);
    }

    validate_arguments(args, lowered);

    Argument me(name(), true, value().type());
    args.push_back(me);

    ofstream src(filename.c_str());
    CodeGen_C cg(src);
    cg.compile(lowered, fn_name.empty() ? name() : fn_name, args);
}

void Func::compile_to_file(const string &filename_prefix, vector<Argument> args) {
    compile_to_header(filename_prefix + ".h", args, filename_prefix);
    compile_to_object(filename_prefix + ".o", args, filename_prefix);
}

void Func::compile_to_file(const string &filename_prefix) {
    compile_to_file(filename_prefix, vector<Argument>());
}

void Func::compile_to_file(const string &filename_prefix, Argument a) {
    compile_to_file(filename_prefix, Internal::vec(a));    
}

void Func::compile_to_file(const string &filename_prefix, Argument a, Argument b) {
    compile_to_file(filename_prefix, Internal::vec(a, b));    
}

void Func::compile_to_file(const string &filename_prefix, Argument a, Argument b, Argument c) {
    compile_to_file(filename_prefix, Internal::vec(a, b, c));    
}

void Func::compile_to_file(const string &filename_prefix, Argument a, Argument b, Argument c, Argument d) {
    compile_to_file(filename_prefix, Internal::vec(a, b, c, d));    
}

void Func::compile_to_file(const string &filename_prefix, Argument a, Argument b, Argument c, Argument d, Argument e) {
    compile_to_file(filename_prefix, Internal::vec(a, b, c, d, e));    
}

void Func::compile_to_assembly(const string &filename, vector<Argument> args, const string &fn_name) {
    assert(value().defined() && "Can't compile undefined function");    

    if (!lowered.defined()) lowered = Halide::Internal::lower(func);
    Argument me(name(), true, value().type());
    args.push_back(me);

    StmtCompiler cg;
    cg.compile(lowered, fn_name.empty() ? name() : fn_name, args);
    cg.compile_to_native(filename, true);
}

void Func::set_error_handler(void (*handler)(char *)) {
    error_handler = handler;
    if (compiled_module.set_error_handler) {
        compiled_module.set_error_handler(handler);
    }
}

void Func::set_custom_allocator(void *(*cust_malloc)(size_t), void (*cust_free)(void *)) {
    custom_malloc = cust_malloc;
    custom_free = cust_free;
    if (compiled_module.set_custom_allocator) {
        compiled_module.set_custom_allocator(cust_malloc, cust_free);
    }
}

void Func::set_custom_do_par_for(void (*cust_do_par_for)(void (*)(int, uint8_t *), int, int, uint8_t *)) {
    custom_do_par_for = cust_do_par_for;
    if (compiled_module.set_custom_do_par_for) {
        compiled_module.set_custom_do_par_for(cust_do_par_for);
    }
}

void Func::set_custom_do_task(void (*cust_do_task)(void (*)(int, uint8_t *), int, uint8_t *)) {
    custom_do_task = cust_do_task;
    if (compiled_module.set_custom_do_task) {
        compiled_module.set_custom_do_task(cust_do_task);
    }
}

void Func::realize(Buffer dst) {
    if (!compiled_module.wrapped_function) compile_jit();

    assert(compiled_module.wrapped_function);

    // Check the type and dimensionality of the buffer
    assert(dst.dimensions() == dimensions() && "Buffer and Func have different dimensionalities");
    assert(dst.type() == value().type() && "Buffer and Func have different element types");

    // In case these have changed since the last realization
    compiled_module.set_error_handler(error_handler);
    compiled_module.set_custom_allocator(custom_malloc, custom_free);   
    compiled_module.set_custom_do_par_for(custom_do_par_for);
    compiled_module.set_custom_do_task(custom_do_task);

    // Update the address of the buffer we're realizing into
    arg_values[arg_values.size()-1] = dst.raw_buffer();

    // Update the addresses of the image param args
    Internal::log(3) << image_param_args.size() << " image param args to set\n";
    for (size_t i = 0; i < image_param_args.size(); i++) {
        Internal::log(3) << "Updating address for image param: " << image_param_args[i].second.name() << "\n";
        Buffer b = image_param_args[i].second.get_buffer();
        assert(b.defined() && "An ImageParam is not bound to a buffer");
        arg_values[image_param_args[i].first] = b.raw_buffer();
    }

    for (size_t i = 0; i < arg_values.size(); i++) {
        Internal::log(2) << "Arg " << i << " = " << arg_values[i] << "\n";
        assert(arg_values[i] != NULL && "An argument to a jitted function is null\n");
    }

    Internal::log(2) << "Calling jitted function\n";
    compiled_module.wrapped_function(&(arg_values[0]));    
    Internal::log(2) << "Back from jitted function\n";

    dst.set_source_module(compiled_module);
}

void *Func::compile_jit() {
    assert(value().defined() && "Can't realize undefined function");
    
    if (!lowered.defined()) lowered = Halide::Internal::lower(func);
    
    // Infer arguments
    InferArguments infer_args;
    lowered.accept(&infer_args);
    
    Argument me(name(), true, value().type());
    infer_args.arg_types.push_back(me);
    arg_values = infer_args.arg_values;
    arg_values.push_back(NULL); // A spot to put the address of the output buffer
    image_param_args = infer_args.image_param_args;
    
    Internal::log(2) << "Inferred argument list:\n";
    for (size_t i = 0; i < infer_args.arg_types.size(); i++) {
        Internal::log(2) << infer_args.arg_types[i].name << ", " 
                         << infer_args.arg_types[i].type << ", " 
                         << infer_args.arg_types[i].is_buffer << "\n";
    }
    
    StmtCompiler cg;
    cg.compile(lowered, name(), infer_args.arg_types);
    
    if (log::debug_level >= 3) {
        cg.compile_to_native(name() + ".s", true);
        cg.compile_to_bitcode(name() + ".bc");
        ofstream stmt_debug((name() + ".stmt").c_str());
        stmt_debug << lowered;
    }
    
    compiled_module = cg.compile_to_function_pointers();    

    return compiled_module.function;
}

void Func::test() {

    Image<int> input(7, 5);
    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
            input(x, y) = x*y + 10/(y+3);
        }
    }


    Func f, g;
    Var x, y;
    f(x, y) = input(x+1, y) + input(x+1, y)*3 + 1;
    g(x, y) = f(x-1, y) + 2*f(x+1, y);
    

    f.compute_root();

    Image<int> result = g.realize(5, 5);

    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
            int correct = (4*input(x, y)+1) + 2*(4*input(x+2, y)+1);
            assert(result(x, y) == correct);
        }
    }

    std::cout << "Func test passed" << std::endl;

}

}
