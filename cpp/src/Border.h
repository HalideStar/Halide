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
#endif

// An ugly part of this code is that it creates some dynamically allocated
// class objects and uses some static ones as well.  This means, quite simply, it leaks memory.

namespace Halide {
namespace Border {

class BorderFunc;
class BorderIndex;

/** Base class for border handlers, especially index manipulating border handlers. */
class BorderBase {
public:    
    // expr: The index expression.
    // min, max: The limits for the border handling of that dimension.
    virtual Expr indexExpr(int dim, Expr expr, Expr min, Expr max) { assert(0 && "Called BorderBase"); return expr; }
    // value: The value of the function expression, typically returned when inside the border.
    virtual Expr valueExpr(int dim, Expr value, Expr expr, Expr min, Expr max) { return value; } // Default: no impact
    
    // dim() returns a border handler pointer specific to a particular dimension.
    BorderBase *dim(int d);
    
    Func operator()(Func in);
};

/** Class to provide access to a particular dimension of a border handler that has dimension-specific behaviour */
class BorderIndex : public BorderBase {
protected:
    BorderBase *base;
    int dim;
    
public:
    BorderIndex() : base(0), dim(0) {}
    BorderIndex(BorderBase *_base, int _dim) : base(_base), dim(_dim) {}
    
    virtual Expr indexExpr(int _dim, Expr expr, Expr min, Expr max) { return base->indexExpr(_dim + dim, expr, min, max); }
    virtual Expr valueExpr(int _dim, Expr value, Expr expr, Expr min, Expr max) { return base->valueExpr(_dim + dim, value, expr, min, max); }
};

/** Border handler class is a simple wrapper for pointers to border base class objects. */
class BorderFunc {
    BorderBase *ptr;
public:
    BorderFunc() : ptr(0) {}
    BorderFunc(BorderBase *p) : ptr(p) {}
    BorderFunc(BorderBase &b) : ptr(&b) {}
    
    // When invoking BorderBase, set initial dim to zero and work from there.
    Expr indexExpr(Expr expr, Expr min, Expr max) { assert(ptr && "Undefined border handler"); return ptr->indexExpr(0, expr, min, max); }
    Expr valueExpr(Expr value, Expr expr, Expr min, Expr max) { assert(ptr && "Undefined border handler"); return ptr->valueExpr(0, value, expr, min, max); }
    BorderFunc dim(int d) { return BorderFunc(new BorderIndex(ptr, d)); }
    
    // Border::replicate(in) means apply replication to all dimensions of in.
    // It only works on Func objects, because we have to get the dimension count.
    Func operator()(Func in) { return ptr->operator()(in); }
};

/** Base class for border handlers that manipulate only the value, not the index. */
class BorderValueBase : public BorderBase {
public:
    // The index must be clamped to avoid out-of-bounds access.
    virtual Expr indexExpr(int dim, Expr expr, Expr min, Expr max) { return clamp(expr, min, max); }
};

/** Border handling expressions. */
Func border(std::vector<BorderFunc> handlers, Func f);

Func border(BorderFunc h1, Func f);

Func border(BorderFunc h1, BorderFunc h2, Func f);

Func border(BorderFunc h1, BorderFunc h2, BorderFunc h3, Func f);

Func border(BorderFunc h1, BorderFunc h2, BorderFunc h3, BorderFunc h4, Func f);

/** A border handler that provides no border. */
class BorderNone : public BorderBase {
public:
    virtual Expr indexExpr(int dim, Expr expr, Expr min, Expr max) { return expr; }
};

/** A border handler that replicates the boundary pixels. */
class BorderReplicate : public BorderBase {
public:
    virtual Expr indexExpr(int dim, Expr expr, Expr min, Expr max) { return clamp(expr, min, max); }
};

/** A border handler that wraps the function around at the borders. */
class BorderWrap : public BorderBase {
public:
    virtual Expr indexExpr(int dim, Expr expr, Expr min, Expr max) { return new Internal::Clamp(Internal::Clamp::Wrap, expr, min, max); }
};

/** A border handler that reflects including the boundary. */
class BorderReflect : public BorderBase {
public:
    virtual Expr indexExpr(int dim, Expr expr, Expr min, Expr max) { return new Internal::Clamp(Internal::Clamp::Reflect, expr, min, max); }
};

/** A border handler that reflects excluding the boundary. */
class BorderReflect101 : public BorderBase {
public:
    virtual Expr indexExpr(int dim, Expr expr, Expr min, Expr max) { return new Internal::Clamp(Internal::Clamp::Reflect101, expr, min, max); }
};

/** A border handler that replaces pixels outside the range with a constant expression. */
class BorderConstant : public BorderValueBase {
    Expr constant; // Can be any Halide data type, really
public:
    BorderConstant() { constant = Expr(); } // There is no default constant: require it to be set.
    BorderConstant(Expr k) : constant(k) {}
    
    virtual Expr valueExpr(int dim, Expr value, Expr expr, Expr min, Expr max) {
        assert(constant.defined() && "Border::constant requires constant value to be specified"); 
        return select(expr < min, constant, select(expr > max, constant, value)); 
    }
    
    /** Write Border::constant(k) to create a border handler with k as the constant expression */
    BorderFunc operator()(Expr k) { return BorderFunc(new BorderConstant(k)); }
};

/** A border handler that replicates tiles of the image at the borders. */
class BorderTile : public BorderBase {
    std::vector<Expr> tile; // The tile dimensions.
public:
    BorderTile() { tile.clear(); } // There is no default tile.
    BorderTile(std::vector<Expr> _tile) : tile(_tile) {}
    
    virtual Expr indexExpr(int dim, Expr expr, Expr min, Expr max) {
        if ((int) tile.size() <= dim) return expr; // No border handling at all on this dimension
        return new Internal::Clamp(Internal::Clamp::Tile, expr, min, max, tile[dim]); 
    }
    
    /** Write Border::tile(t1, t2, ...) to create a border handler with tiling dimensions t1, t2, ...
     * For dimensions with no tile size specified, it behaves as though Border::none were specified.
     */
    BorderFunc operator()(Expr t1) { return BorderFunc(new BorderTile(vec(t1))); }
    BorderFunc operator()(Expr t1, Expr t2) { return BorderFunc(new BorderTile(vec(t1, t2))); }
    BorderFunc operator()(Expr t1, Expr t2, Expr t3) { return BorderFunc(new BorderTile(vec(t1, t2, t3))); }
    BorderFunc operator()(Expr t1, Expr t2, Expr t3, Expr t4) { return BorderFunc(new BorderTile(vec(t1, t2, t3, t4))); }
};

/** The actual instances of border handlers, for use in expressions */
BORDER_EXTERN BorderNone none;
BORDER_EXTERN BorderReplicate replicate;
BORDER_EXTERN BorderWrap wrap;
BORDER_EXTERN BorderReflect reflect;
BORDER_EXTERN BorderReflect101 reflect101;
BORDER_EXTERN BorderConstant constant;
BORDER_EXTERN BorderTile tile;

}

namespace Internal{
void border_test();
}
}
#endif
