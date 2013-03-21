#ifndef HALIDE_BORDER_H
#define HALIDE_BORDER_H

#include "IntrusivePtr.h"
#include "IR.h"
#include "IROperator.h"
#include "Func.h"
#include "Util.h"

#include <vector>

#ifndef BORDER_EXTERN
#define BORDER_EXTERN extern
#define BORDER_EXTERN_INIT(decl,init) extern decl
#define BORDER_EXTERN_CONSTRUCTOR(decl,args) extern decl
#endif

// An ugly part of this code is that it creates some dynamically allocated
// class objects and uses some static ones as well.  This means, quite simply, it leaks memory.

namespace Halide {
namespace Border {

class BorderFunc;

/** Base class for border functions, especially index manipulating border functions. */
class BorderBase {
public:
    // Reference counter for intrusive pointer.
    mutable Internal::RefCount ref_count;
    
    // expr: The index expression.
    // min, max: The limits for the border handling of that dimension.
    // value: The value of the function expression, typically returned when inside the border.
    virtual Expr indexExpr(int dim, Expr expr, Expr min, Expr max) { assert(0 && "Called BorderBase"); return expr; }
    virtual Expr valueExpr(int dim, Expr value, Expr expr, Expr min, Expr max) { return value; } // Default: no impact
    
    // dim() returns a border function specific to a particular dimension.  It uses BorderIndex.
    BorderFunc dim(int d);
    
    // operator() applies the specific border function to a Halide Func.
    // It uses indexExpr and valueExpr of the derived class.
    Func operator()(Func in);
};

/** Border function class is a simple wrapper for pointers to border base class objects.
 * It uses an intrusive pointer to track the underlying BorderBase (or derived) class objects. */
class BorderFunc {
private:
    Internal::IntrusivePtr<BorderBase> contents;
public:
    // Construct an empty BorderFunc object - required for std::vector.
    BorderFunc() : contents(NULL) {}
    // Construct a BorderFunc object that refers to contents pointer.
    // The contents object MUST be created using new.
    BorderFunc(BorderBase *p) : contents(p) {}
    
    // When invoking BorderBase, set initial dim to zero and work from there.
    Expr indexExpr(Expr expr, Expr min, Expr max) { assert(contents.ptr && "Undefined border function"); return contents.ptr->indexExpr(0, expr, min, max); }
    Expr valueExpr(Expr value, Expr expr, Expr min, Expr max) { assert(contents.ptr && "Undefined border function"); return contents.ptr->valueExpr(0, value, expr, min, max); }

    // For use from BorderIndex, provide dim parameter
    Expr indexExpr(int dim, Expr expr, Expr min, Expr max) { assert(contents.ptr && "Undefined border function"); return contents.ptr->indexExpr(dim, expr, min, max); }
    Expr valueExpr(int dim, Expr value, Expr expr, Expr min, Expr max) { assert(contents.ptr && "Undefined border function"); return contents.ptr->valueExpr(dim, value, expr, min, max); }

    // BorderFunc.dim(d) returns a border function with the dimension index set to d.
    // Example is Border::tile(2,3).dim(0).  This is used to split a BorderFunc up into individual dimensions.
    BorderFunc dim(int d);
    
    // Simple application such as Border::replicate(in) means apply border function to all dimensions of in.
    // It uses the dimension of the function to split up the border function into separate dimensions.
    Func operator()(Func in) { return contents.ptr->operator()(in); }
};

/** Class to provide access to a particular dimension of a border function that has dimension-specific behaviour.
 * Pass a BorderFunc to the constructor.  It is copied (so the reference count should be adjusted).
 * Then, when you use the indexExpr and valueExpr accessors, the dimension is adjusted to access the underlying
 * BorderFunc. */
class BorderIndex : public BorderBase {
protected:
    // The base BorderFunc and the dimension of that border function to be accessed
    BorderFunc base;
    int dim;
    
public:
    // Empty constructor for std::vector.
    BorderIndex() : base(0), dim(0) {}
    // Constructor can be used to specify the dimension of interest.
    BorderIndex(BorderFunc _base, int _dim) : base(_base), dim(_dim) {}
    
    // indexExpr and valueExpr can also specify the dimension of interest, which is then
    // relative to the base dimension specified in the constructor call.
    virtual Expr indexExpr(int _dim, Expr expr, Expr min, Expr max) { return base.indexExpr(_dim + dim, expr, min, max); }
    virtual Expr valueExpr(int _dim, Expr value, Expr expr, Expr min, Expr max) { return base.valueExpr(_dim + dim, value, expr, min, max); }
};

/** Base class for borderfuncs that manipulate only the value, not the index. */
class BorderValueBase : public BorderBase {
public:
    // The index must be clamped to avoid out-of-bounds access.
    virtual Expr indexExpr(int dim, Expr expr, Expr min, Expr max) { return clamp(expr, min, max); }
};

/** A border function that uses individual border functions for individual dimensions.
 * e.g. write Border::border(Border::replicate, Border::wrap)(in) to apply replication to dimension 0
 * and wrapping to dimension 1. Border::border() is a wrapper function that builds a BorderGeneral
 * class object. */
class BorderGeneral : public BorderBase {
    // The underlying border functions, one for each dimension.
    std::vector<BorderFunc> borderfuncs;
public:
    // Empty constructor for std::vector.
    BorderGeneral() {}
    // General constructor accepts a vector of border functions
    BorderGeneral(std::vector<BorderFunc> bf) : borderfuncs(bf) {}

    // indexExpr and valueExpr invoke the corresponding accessors in the
    // border function associated with the particular dimension.
    virtual Expr indexExpr(int dim, Expr expr, Expr min, Expr max) { 
        return borderfuncs[dim].indexExpr(expr, min, max); 
    }
    virtual Expr valueExpr(int dim, Expr value, Expr expr, Expr min, Expr max) { 
        return borderfuncs[dim].valueExpr(value, expr, min, max); 
    }
    
    // Apply the border functions to the corresponding dimensions of in.
    Func operator()(Func in);
};

/** Build general border functions: allows expressions of the form 
 *     Border::border(BorderFunc bf1, BorderFunc bf2, ..)(Func f) 
 * but also, Border::border(BorderFunc bf1, BorderFunc bf2, ...) is a BorderFunc that
 * can be passed to a C++ higher-order function.
 */
BorderFunc border(BorderFunc h1);
BorderFunc border(BorderFunc h1, BorderFunc h2);
BorderFunc border(BorderFunc h1, BorderFunc h2, BorderFunc h3);
BorderFunc border(BorderFunc h1, BorderFunc h2, BorderFunc h3, BorderFunc h4);


/** A border function that provides no border. */
class BorderNone : public BorderBase {
public:
    virtual Expr indexExpr(int dim, Expr expr, Expr min, Expr max) { return new Internal::Clamp(Internal::Clamp::None, expr); }
};

/** A border function that replicates the boundary pixels. */
class BorderReplicate : public BorderBase {
public:
    virtual Expr indexExpr(int dim, Expr expr, Expr min, Expr max) { return clamp(expr, min, max); }
};

/** A border function that wraps the function around at the borders. */
class BorderWrap : public BorderBase {
public:
    virtual Expr indexExpr(int dim, Expr expr, Expr min, Expr max) { return new Internal::Clamp(Internal::Clamp::Wrap, expr, min, max); }
};

/** A border function that reflects including the boundary. */
class BorderReflect : public BorderBase {
public:
    virtual Expr indexExpr(int dim, Expr expr, Expr min, Expr max) { return new Internal::Clamp(Internal::Clamp::Reflect, expr, min, max); }
};

/** A border function that reflects excluding the boundary. */
class BorderReflect101 : public BorderBase {
public:
    virtual Expr indexExpr(int dim, Expr expr, Expr min, Expr max) { return new Internal::Clamp(Internal::Clamp::Reflect101, expr, min, max); }
};

/** A border function that replaces pixels outside the range with a constant expression. */
class BorderConstant : public BorderValueBase {
    Expr constant; // Can be any Halide data type, really
public:
    BorderConstant() { constant = Expr(); } // There is no default constant: require it to be set.
    BorderConstant(Expr k) : constant(k) {}
    
    virtual Expr valueExpr(int dim, Expr value, Expr expr, Expr min, Expr max) {
        assert(constant.defined() && "Border::constant requires constant value to be specified"); 
        return select(expr < min, constant, select(expr > max, constant, value)); 
    }
};

/** A border function that replicates tiles of the image at the borders.
 * If the number of tile dimensions specified is less than the image/Func dimensionality,
 * then the additional dimensions are tiled by repetition of the pattern. */
class BorderTile : public BorderBase {
    std::vector<Expr> tile; // The tile dimensions.
public:
    BorderTile() { tile.clear(); } // There is no default tile.
    BorderTile(std::vector<Expr> _tile) : tile(_tile) {}
    
    virtual Expr indexExpr(int dim, Expr expr, Expr min, Expr max) {
        assert(tile.size() > 0 && "BorderTile requires at least one tile dimension");
        dim = dim % ((int) tile.size());
        return new Internal::Clamp(Internal::Clamp::Tile, expr, min, max, tile[dim]); 
    }
    
};

/** The actual instances of border functions, for use in expressions */
BORDER_EXTERN_CONSTRUCTOR(BorderFunc none,(new BorderNone));
BORDER_EXTERN_CONSTRUCTOR(BorderFunc replicate,(new BorderReplicate));
BORDER_EXTERN_CONSTRUCTOR(BorderFunc wrap,(new BorderWrap));
BORDER_EXTERN_CONSTRUCTOR(BorderFunc reflect,(new BorderReflect));
BORDER_EXTERN_CONSTRUCTOR(BorderFunc reflect101,(new BorderReflect101));

/** Where the class object requires parameters, provide C++ functions 
 * the build the needed BorderFunc on the fly instead of prebuilt BorderFuncs. */
/** Write Border::constant(k) to create a border function with k as the constant expression */
BorderFunc constant(Expr k);
/** Write Border::tile(t1, t2, ...) to create a border function with tiling dimensions t1, t2, ...
 * For dimensions with no tile size specified, it behaves as though Border::none were specified.
 */
BorderFunc tile(Expr t1);
BorderFunc tile(Expr t1, Expr t2);
BorderFunc tile(Expr t1, Expr t2, Expr t3);
BorderFunc tile(Expr t1, Expr t2, Expr t3, Expr t4);

}

namespace Internal{
void border_test();
}
}
#endif
