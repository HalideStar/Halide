#ifndef HALIDE_FUNC_H
#define HALIDE_FUNC_H

/** \file
 * 
 * Defines Func - the front-end handle on a halide function, and related classes.
 */

#include "IR.h"
#include "Var.h"
#include "IntrusivePtr.h"
#include "Function.h"
#include "Param.h"
#include "Argument.h"
#include "RDom.h"
#include "JITCompiledModule.h"
// LH
#include "Image.h"
#include "Util.h"
#include "DomainInference.h"

namespace Halide {
        
/** A fragment of front-end syntax of the form f(x, y, z), where x,
 * y, z are Vars. It could be the left-hand side of a function
 * definition, or it could be a call to a function. We don't know
 * until we see how this object gets used.
 */
class FuncRefVar {
    Internal::Function func;
    std::vector<std::string> args;
    void add_implicit_vars(std::vector<std::string> &args, Expr e);
public:
    FuncRefVar(Internal::Function, const std::vector<Var> &);
        
    /**  Use this as the left-hand-side of a definition */
    void operator=(Expr);

    /** Define this function as a sum reduction over the negative of
     * the given expression. The expression should refer to some RDom
     * to sum over. If the function does not already have a pure
     * definition, this sets it to zero.
     */
    void operator+=(Expr);

    /** Define this function as a sum reduction over the negative of
     * the given expression. The expression should refer to some RDom
     * to sum over. If the function does not already have a pure
     * definition, this sets it to zero.
     */
    void operator-=(Expr);

    /** Define this function as a product reduction. The expression
     * should refer to some RDom to take the product over. If the
     * function does not already have a pure definition, this sets it
     * to 1.
     */    
    void operator*=(Expr);

    /** Define this function as the product reduction over the inverse
     * of the expression. The expression should refer to some RDom to
     * take the product over. If the function does not already have a
     * pure definition, this sets it to 1. 
     */
    void operator/=(Expr);

    /** Override the usual assignment operator, so that 
     * f(x, y) = g(x, y) defines f. 
     */
    void operator=(const FuncRefVar &e) {*this = Expr(e);}
        
    /** Use this FuncRefVar as a call to the function, and not as the
     * left-hand-side of a definition. 
     */
    operator Expr() const;
};
    
/** A fragment of front-end syntax of the form f(x, y, z), where x, y,
 * z are Exprs. If could be the left hand side of a reduction
 * definition, or it could be a call to a function. We don't know
 * until we see how this object gets used.
 */
class FuncRefExpr {
    Internal::Function func;
    std::vector<Expr> args;
    void add_implicit_vars(std::vector<Expr> &args, Expr e);
public:
    FuncRefExpr(Internal::Function, const std::vector<Expr> &);
    FuncRefExpr(Internal::Function, const std::vector<std::string> &);
        
    /** Use this as the left-hand-side of a reduction definition (see
     * \ref RDom). The function must already have a pure definition.
     */
    void operator=(Expr);

    /** Define this function as a sum reduction over the negative of
     * the given expression. The expression should refer to some RDom
     * to sum over. If the function does not already have a pure
     * definition, this sets it to zero.
     */
    void operator+=(Expr);

    /** Define this function as a sum reduction over the negative of
     * the given expression. The expression should refer to some RDom
     * to sum over. If the function does not already have a pure
     * definition, this sets it to zero.
     */
    void operator-=(Expr);

    /** Define this function as a product reduction. The expression
     * should refer to some RDom to take the product over. If the
     * function does not already have a pure definition, this sets it
     * to 1.
     */    
    void operator*=(Expr);

    /** Define this function as the product reduction over the inverse
     * of the expression. The expression should refer to some RDom to
     * take the product over. If the function does not already have a
     * pure definition, this sets it to 1. 
     */
    void operator/=(Expr);

    /* Override the usual assignment operator, so that
     * f(x, y) = g(x, y) defines f. 
     */
    void operator=(const FuncRefExpr &e) {*this = Expr(e);}
        
    /** Use this as a call to the function, and not the left-hand-side
     * of a definition. */
    operator Expr() const;
};

/** A wrapper around a schedule used for common schedule manipulations */
class ScheduleHandle {
    Internal::Schedule &schedule;
    void set_dim_type(Var var, Internal::For::ForType t);
public:
    ScheduleHandle(Internal::Schedule &s) : schedule(s) {}

    /** Split a dimension into inner and outer subdimensions with the
     * given names, where the inner dimension iterates from 0 to
     * factor-1. The inner and outer subdimensions can then be dealt
     * with using the other scheduling calls. It's ok to reuse the old
     * variable name as either the inner or outer variable. */
    ScheduleHandle &split(Var old, Var outer, Var inner, Expr factor);

    /** Mark a dimension to be traversed in parallel */
    ScheduleHandle &parallel(Var var);

    /** Mark a dimension to be computed all-at-once as a single
     * vector. The dimension should have constant extent -
     * e.g. because it is the inner dimension following a split by a
     * constant factor. For most uses of vectorize you want the two
     * argument form. The variable to be vectorized should be the
     * innermost one. */
    ScheduleHandle &vectorize(Var var);

    /** Mark a dimension to be completely unrolled. The dimension
     * should have constant extent - e.g. because it is the inner
     * dimension following a split by a constant factor. For most uses
     * of unroll you want the two-argument form. */
    ScheduleHandle &unroll(Var var);

    /** Split a dimension by the given factor, then vectorize the
     * inner dimension. This is how you vectorize a loop of unknown
     * size. The variable to be vectorized should be the innermost
     * one. After this call, var refers to the outer dimension of the
     * split. */
    ScheduleHandle &vectorize(Var var, int factor);

    /** Split a dimension by the given factor, then unroll the inner
     * dimension. This is how you unroll a loop of unknown size by
     * some constant factor. After this call, var refers to the outer
     * dimension of the split. */
    ScheduleHandle &unroll(Var var, int factor);

    /** Statically declare that the range over which a function should
     * be evaluated is given by the second and third arguments. This
     * can let Halide perform some optimizations. E.g. if you know
     * there are going to be 4 color channels, you can completely
     * vectorize the color channel dimension without the overhead of
     * splitting it up. If bounds inference decides that it requires
     * more of this function than the bounds you have stated, a
     * runtime error will occur when you try to run your pipeline. */
    ScheduleHandle &bound(Var var, Expr min, Expr extent);

    /** Split two dimensions at once by the given factors, and then
     * reorder the resulting dimensions to be xi, yi, xo, yo from
     * innermost outwards. This gives a tiled traversal. */
    ScheduleHandle &tile(Var x, Var y, Var xo, Var yo, Var xi, Var yi, Expr xfactor, Expr yfactor);

    /** A shorter form of tile, which reuses the old variable names as
     * the new outer dimensions */
    ScheduleHandle &tile(Var x, Var y, Var xi, Var yi, Expr xfactor, Expr yfactor);

    /** Reorder two dimensions so that x is traversed inside y. Does
     * not affect the nesting order of other dimensions. E.g, if you
     * say foo(x, y, z, w) = bar; foo.reorder(w, x); then foo will be
     * traversed in the order (w, y, z, x), from innermost
     * outwards. */
    ScheduleHandle &reorder(Var x, Var y);

    /** Reorder three dimensions to have the given nesting order, from
     * innermost out */
    ScheduleHandle &reorder(Var x, Var y, Var z);

    /** Reorder four dimensions to have the given nesting order, from
     * innermost out */
    ScheduleHandle &reorder(Var x, Var y, Var z, Var w);

    /** Reorder five dimensions to have the given nesting order, from
     * innermost out */
    ScheduleHandle &reorder(Var x, Var y, Var z, Var w, Var t);
};

/** A halide function. This class represents one stage in a Halide
 * pipeline, and is the unit by which we schedule things. By default
 * they are aggressively inlined, so you are encouraged to make lots
 * of little functions, rather than storing things in Exprs. */
class Func {
    
    /** A handle on the internal halide function that this
     * represents */
    Internal::Function func;

    /** When you make a reference to this function to with fewer
     * arguments that it has dimensions, the argument list is bulked
     * up with 'implicit' vars with canonical names. This lets you
     * pass around partially-applied halide functions. */
    // @{
    void add_implicit_vars(std::vector<Var> &);
    void add_implicit_vars(std::vector<Expr> &);
    // @}

    /** A JIT-compiled version of this function that we save so that
     * we don't have to rejit every time we want to evaluated it. */
    Internal::JITCompiledModule compiled_module;

    /** The current error handler used for realizing this
     * function. May be NULL. Only relevant when jitting. */
    void (*error_handler)(char *);

    /** The current custom allocator used for realizing this
     * function. May be NULL. Only relevant when jitting. */
    // @{
    void *(*custom_malloc)(size_t);
    void (*custom_free)(void *);
    // @}

    /** Pointers to current values of the automatically inferred
     * arguments (buffers and scalars) used to realize this
     * function. Only relevant when jitting. We can hold these things
     * with raw pointers instead of reference-counted handles, because
     * func indirectly holds onto them with reference-counted handles
     * via its value Expr. */
    std::vector<const void *> arg_values;

    /** Some of the arg_values need to be rebound on every call if the
     * image params change. The pointers for the scalar params will
     * still be valid though. */
    std::vector<std::pair<int, Internal::Parameter> > image_param_args;

public:        
    static void test();

    /** Declare a new undefined function with the given name */
    Func(const std::string &name);

    /** Declare a new undefined function with an
     * automatically-generated unique name */
    Func();

    /** Declare a new function with an automatically-generated unique
     * name, and define it to return the given expression (which may
     * not contain free variables). */
    Func(Expr e);

    /** Evaluate this function over some rectangular domain and return
     * the resulting buffer. The buffer should probably be instantly
     * wrapped in an Image class of the appropriate type. That is, do
     * this: 
     * 
     * Image<float> im = f.realize(...);
     * 
     * not this:
     * 
     * Buffer im = f.realize(...) 
     *
     */
    Buffer realize(int x_size = 0, int y_size = 0, int z_size = 0, int w_size = 0);

    /** Evaluate this function into an existing allocated buffer. If
     * the buffer is also one of the arguments to the function,
     * strange things may happen, as the pipeline isn't necessarily
     * safe to run in-place. */
    void realize(Buffer dst);

    /** Statically compile this function to llvm bitcode, with the
     * given filename (which should probably end in .bc), type
     * signature, and C function name (which defaults to the same name
     * as this halide function */
    void compile_to_bitcode(const std::string &filename, std::vector<Argument>, const std::string &fn_name = "");

    /** Statically compile this function to an object file, with the
     * given filename (which should probably end in .o or .obj), type
     * signature, and C function name (which defaults to the same name
     * as this halide function. You probably don't want to use this directly - instead call compile_to_file.  */
    void compile_to_object(const std::string &filename, std::vector<Argument>, const std::string &fn_name = "");

    /** Emit a header file with the given filename for this
     * function. The header will define a function with the type
     * signature given by the second argument, and a name given by the
     * third. The name defaults to the same name as this halide
     * function. You don't actually have to have defined this function
     * yet to call this. You probably don't want to use this directly
     * - instead call compile_to_file. */
    void compile_to_header(const std::string &filename, std::vector<Argument>, const std::string &fn_name = "");

    /** Statically compile this function to text assembly equivalent
     * to the object file generated by compile_to_object. This is
     * useful for checking what Halide is producing without having to
     * disassemble anything, or if you need to feed the assembly into
     * some custom toolchain to produce an object file (e.g. iOS) */
    void compile_to_assembly(const std::string &filename, std::vector<Argument>, const std::string &fn_name = "");    
    
    /** Compile to object file and header pair, with the given
     * arguments. Also names the C function to match the first
     * argument. 
     */
    //@{
    void compile_to_file(const std::string &filename_prefix, std::vector<Argument> args);
    void compile_to_file(const std::string &filename_prefix);
    void compile_to_file(const std::string &filename_prefix, Argument a);
    void compile_to_file(const std::string &filename_prefix, Argument a, Argument b);
    void compile_to_file(const std::string &filename_prefix, Argument a, Argument b, Argument c);
    void compile_to_file(const std::string &filename_prefix, Argument a, Argument b, Argument c, Argument d);
    void compile_to_file(const std::string &filename_prefix, Argument a, Argument b, Argument c, Argument d, Argument e);
    // @}

    /** Eagerly jit compile the function to machine code. This
     * normally happens on the first call to realize. If you're
     * running your halide pipeline inside time-sensitive code and
     * wish to avoid including the time taken to compile a pipeline,
     * then you can call this ahead of time. */
    void compile_jit();

    /** Set the error handler function that be called in the case of
     * runtime errors during subsequent calls to realize. Only
     * relevant when jitting. */
    void set_error_handler(void (*handler)(char *));

    /** Set a custom malloc and free to use for subsequent calls to
     * realize. Malloc should return 32-byte aligned chunks of memory,
     * with 32-bytes extra allocated on the start and end so that
     * vector loads can spill off the end slightly. Metadata (e.g. the
     * base address of the region allocated) can go in this margin -
     * it is only read, not written. Only relevant when jitting. */
    void set_custom_allocator(void *(*malloc)(size_t), void (*free)(void *));

    /** When this function is compiled, include code that dumps it to
     * a file after it is realized, for the purpose of debugging. The
     * file format is as follows: 
     * 
     * First, a 20 byte-header containing four little-endian 32-bit
     * ints giving the extents of the first four
     * dimensions. Dimensions beyond four are folded into the
     * fourth. Then, a fifth 32-bit int giving the data type of the
     * function. The typecodes are given by: float = 0, double = 1,
     * uint8_t = 2, int8_t = 3, uint16_t = 4, uint32_t = 5, int32_t =
     * 6, uint64_t = 7, int64_t = 8. The data follows the header, as a
     * densely packed array of the given size and the given type. If
     * given the extension .tmp, this file format can be natively read
     * by the program ImageStack. */
    void debug_to_file(const std::string &filename);

    /** The name of this function, either given during construction,
     * or automatically generated. */
    const std::string &name() const;

    /** The right-hand-side value of the pure definition of this
     * function. May be undefined if the function has no pure
     * definition yet. */
    Expr value() const;

    /** The dimensionality (number of arguments) of this
     * function. Zero if the function is not yet defined. */
    int dimensions() const;

    /** Construct either the left-hand-side of a definition, or a call
     * to a functions that happens to only contain vars as
     * arguments. If the function has already been defined, and fewer
     * arguments are given than the function has dimensions, then
     * enough implicit vars are added to the end of the argument list
     * to make up the difference (see \ref Var::implicit) */
    // @{
    FuncRefVar operator()();
    FuncRefVar operator()(Var x);
    FuncRefVar operator()(Var x, Var y);
    FuncRefVar operator()(Var x, Var y, Var z);
    FuncRefVar operator()(Var x, Var y, Var z, Var w);
    FuncRefVar operator()(std::vector<Var>);
    // @}

    /** Either calls to the function, or the left-hand-side of a
     * reduction definition (see \ref RDom). If the function has
     * already been defined, and fewer arguments are given than the
     * function has dimensions, then enough implicit vars are added to
     * the end of the argument list to make up the difference. (see
     * \ref Var::implicit)*/
    // @{
    FuncRefExpr operator()(Expr x);
    FuncRefExpr operator()(Expr x, Expr y);
    FuncRefExpr operator()(Expr x, Expr y, Expr z);
    FuncRefExpr operator()(Expr x, Expr y, Expr z, Expr w);
    FuncRefExpr operator()(std::vector<Expr>);
    // @}

    /** Scheduling calls that control how the domain of this function
     * is traversed. See the documentation for ScheduleHandle for the meanings */
    // @{
    Func &split(Var old, Var outer, Var inner, Expr factor);
    Func &parallel(Var var);
    Func &vectorize(Var var);
    Func &unroll(Var var);
    Func &vectorize(Var var, int factor);
    Func &unroll(Var var, int factor);
    Func &bound(Var var, Expr min, Expr extent);
    Func &tile(Var x, Var y, Var xo, Var yo, Var xi, Var yi, Expr xfactor, Expr yfactor);
    Func &tile(Var x, Var y, Var xi, Var yi, Expr xfactor, Expr yfactor);
    Func &reorder(Var x, Var y);
    Func &reorder(Var x, Var y, Var z);
    Func &reorder(Var x, Var y, Var z, Var w);
    Func &reorder(Var x, Var y, Var z, Var w, Var t);
    // @}

    /** Compute this function as needed for each unique value of the
     * given var for the given calling function f.
     * 
     * For example, consider the simple pipeline:
     \code    
     Func f, g;
     Var x, y;
     g(x, y) = x*y;
     f(x, y) = g(x, y) + g(x, y+1) + g(x+1, y) + g(x+1, y+1);
     \endcode
     * 
     * If we schedule f like so:
     * 
     \code 
     g.compute_at(f, x); 
     \endcode
     *
     * Then the C code equivalent to this pipeline will look like this
     * 
     \code
     
     int f[height][width];
     for (int y = 0; y < height; y++) {
         for (int x = 0; x < width; x++) {
             int g[2][2];
             g[0][0] = x*y;
             g[0][1] = (x+1)*y;
             g[1][0] = x*(y+1);
             g[1][1] = (x+1)*(y+1);
             f[y][x] = g[0][0] + g[1][0] + g[0][1] + g[1][1];
         }
     }

     \endcode
     * 
     * The allocation and computation of g is within f's loop over x,
     * and enough of g is computed to satisfy all that f will need for
     * that iteration. This has excellent locality - values of g are
     * used as soon as they are computed, but it does redundant
     * work. Each value of g ends up getting computed four times. If
     * we instead schedule f like so:
     * 
     \code      
     g.compute_at(f, y);     
     \endcode
     * 
     * The equivalent C code is: 
     * 
     \code
     int f[height][width];
     for (int y = 0; y < height; y++) {
         int g[2][width+1]; 
         for (int x = 0; x < width; x++) {
             g[0][x] = x*y;
             g[1][x] = x*(y+1);
         }
         for (int x = 0; x < width; x++) {
             f[y][x] = g[0][x] + g[1][x] + g[0][x+1] + g[1][x+1];
         }
     }     
     \endcode
     * 
     * The allocation and computation of g is within f's loop over y,
     * and enough of g is computed to satisfy all that f will need for
     * that iteration. This does less redundant work (each point in g
     * ends up being evaluated twice), but the locality is not quite
     * as good, and we have to allocate more temporary memory to store
     * g.
     */
    Func &compute_at(Func f, Var var);

    /** Schedule a function to be computed within the iteration over
     * some dimension of a reduction domain. Produces equivalent code
     * to the version of compute_at that takes a Var. */
    Func &compute_at(Func f, RVar var);

    /** Compute all of this function once ahead of time. Reusing
     * the example in \ref Func::compute_at :
     *
     \code 
     Func f, g;
     Var x, y;
     g(x, y) = x*y;
     f(x, y) = g(x, y) + g(x, y+1) + g(x+1, y) + g(x+1, y+1);

     g.compute_root();
     \endcode
     * 
     * is equivalent to
     * 
     \code
     int f[height][width];
     int g[height+1][width+1];
     for (int y = 0; y < height+1; y++) {
         for (int x = 0; x < width+1; x++) {
             g[y][x] = x*y;
         }
     }
     for (int y = 0; y < height; y++) {
         for (int x = 0; x < width; x++) {
             f[y][x] = g[y][x] + g[y+1][x] + g[y][x+1] + g[y+1][x+1];
         }
     }          
     \endcode
     * 
     * g is computed once ahead of time, and enough is computed to
     * satisfy all uses of it. This does no redundant work (each point
     * in g is evaluated once), but has poor locality (values of g are
     * probably not still in cache when they are used by f), and
     * allocates lots of temporary memory to store g.
     */
    Func &compute_root();

    /** Allocate storage for this function within f's loop over
     * var. Scheduling storage is optional, and can be used to
     * separate the loop level at which storage occurs from the loop
     * level at which computation occurs to trade off between locality
     * and redundant work. This can open the door for two types of
     * optimization.
     * 
     * Consider again the pipeline from \ref Func::compute_at :
     \code
     Func f, g;
     Var x, y;
     g(x, y) = x*y;
     f(x, y) = g(x, y) + g(x+1, y) + g(x, y+1) + g(x+1, y+1);
     \endcode
     * 
     * If we schedule it like so:
     * 
     \code
     g.compute_at(f, x).store_at(f, y);
     \endcode
     * 
     * Then the computation of g takes place within the loop over x,
     * but the storage takes place within the loop over y:
     *
     \code 
     int f[height][width];
     for (int y = 0; y < height; y++) {
         int g[2][width+1]; 
         for (int x = 0; x < width; x++) {
             g[0][x] = x*y;
             g[0][x+1] = (x+1)*y;
             g[1][x] = x*(y+1);
             g[1][x+1] = (x+1)*(y+1);
             f[y][x] = g[0][x] + g[1][x] + g[0][x+1] + g[1][x+1];
         }
     }          
     \endcode
     * 
     * Provided the for loop over x is serial, halide then
     * automatically performs the following sliding window
     * optimization:
     * 
     \code 
     int f[height][width];
     for (int y = 0; y < height; y++) {
         int g[2][width+1]; 
         for (int x = 0; x < width; x++) {
             if (x == 0) {
                 g[0][x] = x*y;
                 g[1][x] = x*(y+1);
             }
             g[0][x+1] = (x+1)*y;
             g[1][x+1] = (x+1)*(y+1);
             f[y][x] = g[0][x] + g[1][x] + g[0][x+1] + g[1][x+1];
         }
     }          
     \endcode
     * 
     * Two of the assignments to g only need to be done when x is
     * zero. The rest of the time, those sites have already been
     * filled in by a previous iteration. This version has the
     * locality of compute_at(f, x), but allocates more memory and
     * does much less redundant work.
     *
     * Halide then further optimizes this pipeline like so:
     * 
     \code
     int f[height][width];
     for (int y = 0; y < height; y++) {
         int g[2][2]; 
         for (int x = 0; x < width; x++) {
             if (x == 0) {
                 g[0][0] = x*y;
                 g[1][0] = x*(y+1);
             }
             g[0][(x+1)%2] = (x+1)*y;
             g[1][(x+1)%2] = (x+1)*(y+1);
             f[y][x] = g[0][x%2] + g[1][x%2] + g[0][(x+1)%2] + g[1][(x+1)%2];
         }
     }          
     \endcode
     * 
     * Halide has detected that it's possible to use a circular buffer
     * to represent g, and has reduced all accesses to g modulo 2 in
     * the x dimension. This optimization only triggers if the for
     * loop over x is serial, and if halide can statically determine
     * some power of two large enough to cover the range needed. For
     * powers of two, the modulo operator compiles to more efficient
     * bit-masking. This optimization reduces memory usage, and also
     * improves locality by reusing recently-accessed memory instead
     * of pulling new memory into cache.
     *
     */
    Func &store_at(Func f, Var var);

    /** Equivalent to the version of store_at that takes a Var, but
     * schedules storage within the loop over a dimension of a
     * reduction domain */
    Func &store_at(Func f, RVar var);

    /** Equivalent to \ref Func::store_at, but schedules storage
     * outside the outermost loop. */
    Func &store_root();

    /** Aggressively inline all uses of this function. This is the
     * default schedule, so you're unlikely to need to call this. For
     * a reduction, that means it gets computed as close to the
     * innermost loop as possible.
     *
     * Consider once more the pipeline from \ref Func::compute_at :
     * 
     \code
     Func f, g;
     Var x, y;
     g(x, y) = x*y;
     f(x, y) = g(x, y) + g(x+1, y) + g(x, y+1) + g(x+1, y+1);
     \endcode
     * 
     * Leaving g as inline, this compiles to code equivalent to the following C:
     * 
     \code 
     int f[height][width];
     for (int y = 0; y < height; y++) {
         for (int x = 0; x < width; x++) {
             f[y][x] = x*y + x*(y+1) + (x+1)*y + (x+1)*(y+1);
         }
     }   
     \endcode
     */
    Func &compute_inline();
    
    /** Get a handle on the update step of a reduction for the
     * purposes of scheduling it. Only the pure dimensions of the
     * update step can be meaningfully manipulated (see \ref RDom) */
    ScheduleHandle update();

    /** Get a handle on the internal halide function that this Func
     * represents. Useful if you want to do introspection on Halide
     * functions */
    Internal::Function function() const {return func;}

    /** Casting a function to an expression is equivalent to calling
     * the function with zero arguments. Implicit variables will be
     * injected according to the function's dimensionality 
     * (see \ref Var::implicit).
     * 
     * Combined with Func::operator=, this lets you write things like:
     * 
     \code
     Func f, g;
     Var x;
     g(x) = ...     
     f = g * 2;
     \endcode
     *
     */
    operator Expr() {
        return (*this)();
    }

    /** Define a function to take a number of arguments according to
     * the implicit variables present in the given expression, and
     * return the given expression. The expression may not have free
     * variables. */
    void operator=(Expr e) {
        (*this)() = e;
    }

    // LH Extensions 
    // Extension to use Image<T> as a Func object without explicit conversion.
    // This constructor actually builds a mini function.
    template <typename T>
    Func(Image<T> image) : func(Internal::unique_name("image")), error_handler(NULL), custom_malloc(NULL), custom_free(NULL)
    {
        Var x("x"), y("y");

        operator()(x,y) = image(x,y);
        return;
    }
	
	//LH
	/** Get a handle to the valid domain for the purpose of modifying it */
    // It is questionable whether the domain information should be blindly copied across
    // or whether there should be some rearrangement of the index variables according to the
    // way that the caller uses the callee.  The real problem here is that explicitly setting the 
    // domain is too low level.  We need higher-level semantic operations for programmers
    // that are related to what they are actually writing - such as kernel operations, border
    // handling etc.
	Domain &valid();

	//LH
	/** Get a handle to the valid domain for the purpose of inspecting it */
	const Domain &valid() const;
	
	//LH
	/** Set the valid domain in a schedule format */
	Func &valid(Domain d);

	//LH
	/** Set the valid domain to be the same as an existing Func in a schedule format */
	Func &valid(Func f);

	//LH
	/** Get a handle to the computable domain for the purpose of modifying it */
	Domain &computable();

	//LH
	/** Get a handle to the computable domain for the purpose of inspecting it */
	const Domain &computable() const;

	//LH
	/** Set the computable domain in a schedule format */
	Func &computable(Domain d);

	//LH
	/** Set the computable domain to be the same as an existing Func in a schedule format */
	Func &computable(Func f);

    //LH
    /** Return an infinite domain for the current function. */
    // Note: Does not return a reference since that would require creating an object
    // in the Func.  This will be used infrequently, so simply copy the temporary object into
    // the destination.
    Domain infinite();
    
    //LH
    /** Methods to indicate that the current function is a kernel of other functions. */
    Func &kernel(Func f1);
    Func &kernel(Func f1, Func f2);
    Func &kernel(Func f1, Func f2, Func f3);
    Func &kernel(Func f1, Func f2, Func f3, Func f4);
};


}

#endif
