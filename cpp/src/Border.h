#ifndef HALIDE_BORDER_H
#define HALIDE_BORDER_H

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
class BorderIndex;

/** Base class for border functions, especially index manipulating border functions. */
class BorderBase {
public:    
    // expr: The index expression.
    // min, max: The limits for the border handling of that dimension.
    virtual Expr indexExpr(int dim, Expr expr, Expr min, Expr max) { assert(0 && "Called BorderBase"); return expr; }
    // value: The value of the function expression, typically returned when inside the border.
    virtual Expr valueExpr(int dim, Expr value, Expr expr, Expr min, Expr max) { return value; } // Default: no impact
    
    // dim() returns a border function pointer specific to a particular dimension.
    BorderBase *dim(int d);
    
    Func operator()(Func in);
};

/** Class to provide access to a particular dimension of a border function that has dimension-specific behaviour */
class BorderIndex : public BorderBase {
protected:
    BorderBase *base;
    int dim;
    
public:
    BorderIndex() : base(0), dim(0) {}
    // Constructor can be used to specify the dimension of interest.
    BorderIndex(BorderBase *_base, int _dim) : base(_base), dim(_dim) {}
    
    // indexExpr and valueExpr can also specify the dimension of interest, which is then
    // relative to the base dimension specified in the constructor call.
    virtual Expr indexExpr(int _dim, Expr expr, Expr min, Expr max) { return base->indexExpr(_dim + dim, expr, min, max); }
    virtual Expr valueExpr(int _dim, Expr value, Expr expr, Expr min, Expr max) { return base->valueExpr(_dim + dim, value, expr, min, max); }
};

/** Border function class is a simple wrapper for pointers to border base class objects. */
class BorderFunc {
    BorderBase *ptr;
public:
    BorderFunc() : ptr(0) {}
    BorderFunc(BorderBase *p) : ptr(p) {}
    BorderFunc(BorderBase &b) : ptr(&b) {}
    
    // When invoking BorderBase, set initial dim to zero and work from there.
    Expr indexExpr(Expr expr, Expr min, Expr max) { assert(ptr && "Undefined border function"); return ptr->indexExpr(0, expr, min, max); }
    Expr valueExpr(Expr value, Expr expr, Expr min, Expr max) { assert(ptr && "Undefined border function"); return ptr->valueExpr(0, value, expr, min, max); }
    // BorderFunc.dim(d) returns a border function with the dimension index set to d.
    // Example is Border::tile(2,3).dim(0)
    BorderFunc dim(int d) { return BorderFunc(new BorderIndex(ptr, d)); }
    
    // Border::replicate(in) means apply replication to all dimensions of in.
    // It only works on Func objects, because we have to get the dimension count.
    Func operator()(Func in) { return ptr->operator()(in); }
};

/** Base class for borderfuncs that manipulate only the value, not the index. */
class BorderValueBase : public BorderBase {
public:
    // The index must be clamped to avoid out-of-bounds access.
    virtual Expr indexExpr(int dim, Expr expr, Expr min, Expr max) { return clamp(expr, min, max); }
};

/** A border function that uses individual border functions for individual dimensions. */
class BorderGeneral : public BorderBase {
    std::vector<BorderFunc> borderfuncs;
public:
    BorderGeneral() {}
    BorderGeneral(std::vector<BorderFunc> bf) : borderfuncs(bf) {}

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
    virtual Expr indexExpr(int dim, Expr expr, Expr min, Expr max) { return expr; }
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

/** A border function that replicates tiles of the image at the borders. */
class BorderTile : public BorderBase {
    std::vector<Expr> tile; // The tile dimensions.
public:
    BorderTile() { tile.clear(); } // There is no default tile.
    BorderTile(std::vector<Expr> _tile) : tile(_tile) {}
    
    virtual Expr indexExpr(int dim, Expr expr, Expr min, Expr max) {
        if ((int) tile.size() <= dim) return expr; // No border handling at all on this dimension
        return new Internal::Clamp(Internal::Clamp::Tile, expr, min, max, tile[dim]); 
    }
    
};

/** The actual instances of border borderfuncs, for use in expressions */
BORDER_EXTERN_CONSTRUCTOR(BorderFunc none,(new BorderNone));
BORDER_EXTERN_CONSTRUCTOR(BorderFunc replicate,(new BorderReplicate));
BORDER_EXTERN_CONSTRUCTOR(BorderFunc wrap,(new BorderWrap));
BORDER_EXTERN_CONSTRUCTOR(BorderFunc reflect,(new BorderReflect));
BORDER_EXTERN_CONSTRUCTOR(BorderFunc reflect101,(new BorderReflect101));

/** Where the class object requires parameters, provide C++ functions to instantiate. */
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
