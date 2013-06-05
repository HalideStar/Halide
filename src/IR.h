#ifndef HALIDE_IR_H
#define HALIDE_IR_H

/** \file
 * Halide expressions (\ref Halide::Expr) and statements (\ref Halide::Internal::Stmt)
 */

#include <string>
#include <vector>

#include "Buffer.h"
#include "IRVisitor.h"
#include "Type.h"
#include "IntrusivePtr.h"
#include "Util.h"

//LH
/* --------------------------------------
 * How to add tree nodes to the IR.
 *
 * 1. Add the node itself in this file.  Add to the templates in IR.cpp.
 * 2. Add the node to the two lists in IRVisitor.h and an implementation in IRVisitor.cpp.
 * 3. Add the node to the list in IRPrinter.h and define a visitor for it in IRPrinter.cpp.
 * 4. Add a visitor in IRMutator.h and IRMutator.cpp
 * 5. Add to IREquality.cpp
 * 6. If code generation is required (usually, it is); add visitor to CodeGen.cpp and CodeGen.h
 *    If code generation is not required because the node should not remain at code generation
 *    time, should add a visitor and assert an error.
 * 7. If the node appears in code subject to bounds analysis, add to Bounds.h and Bounds.cpp.
 */

namespace Halide {

namespace Internal {

/** Assert that two operands are both defined and of the same type */
void assert_defined_same_type(std::string opname, Expr a, Expr b);
/** Assert that two operands are of the same type if both are defined */
void assert_same_type(std::string opname, Expr a, Expr b);

/** A class representing a type of IR node (e.g. Add, or Mul, or
 * PrintStmt). We use it for rtti (without having to compile with
 * rtti). */
struct IRNodeType {};

/** The abstract base classes for a node in the Halide IR. */
struct IRNode {
    
    /** We use the visitor pattern to traverse IR nodes throughout the
     * compiler, so we have a virtual accept method which accepts
     * visitors.
     */
    virtual void accept(IRVisitor *v) const = 0;
    IRNode() {}
    virtual ~IRNode() {}

    /** These classes are all managed with intrusive reference
       counting, so we also track a reference count. It's mutable
       so that we can do reference counting even through const
       references to IR nodes. */
    mutable RefCount ref_count;

    /** Each IR node subclass should return some unique pointer. We
     * can compare these pointers to do runtime type
     * identification. We don't compile with rtti because that
     * injects run-time type identification stuff everywhere (and
     * often breaks when linking external libraries compiled
     * without it), and we only want it for IR nodes. */
    virtual const IRNodeType *type_info() const = 0;
};

template<>
EXPORT inline RefCount &ref_count<IRNode>(const IRNode *n) {return n->ref_count;}

template<>
EXPORT inline void destroy<IRNode>(const IRNode *n) {delete n;}

/** IR nodes are split into expressions and statements. These are
   similar to expressions and statements in C - expressions
   represent some value and have some type (e.g. x + 3), and
   statements are side-effecting pieces of code that do not
   represent a value (e.g. assert(x > 3)) */

/** A base class for statement nodes. They have no properties or
   methods beyond base IR nodes for now */
struct BaseStmtNode : public IRNode {
};

/** A base class for expression nodes. They all contain their types
 * (e.g. Int(32), Float(32)) */
struct BaseExprNode : public IRNode {
    Type type;
};

/** We use the "curiously recurring template pattern" to avoid
   duplicated code in the IR Nodes. These classes live between the
   abstract base classes and the actual IR Nodes in the
   inheritance hierarchy. It provides an implementation of the
   accept function necessary for the visitor pattern to work, and
   a concrete instantiation of a unique IRNodeType per class. */
template<typename T>
struct ExprNode : public BaseExprNode {
    void accept(IRVisitor *v) const {
        v->visit((const T *)this);
    }
    virtual IRNodeType *type_info() const {return &_type_info;}
    static EXPORT IRNodeType _type_info;
};

template<typename T>
struct StmtNode : public BaseStmtNode {
    void accept(IRVisitor *v) const {
        v->visit((const T *)this);
    }
    virtual IRNodeType *type_info() const {return &_type_info;}
    static EXPORT IRNodeType _type_info;
};
    
/** IR nodes are passed around opaque handles to them. This is a
   base class for those handles. It manages the reference count,
   and dispatches visitors. */
struct IRHandle : public IntrusivePtr<const IRNode> {
    IRHandle() : IntrusivePtr<const IRNode>() {}
    IRHandle(const IRNode *p) : IntrusivePtr<const IRNode>(p) {}

    /** Dispatch to the correct visitor method for this node. E.g. if
     * this node is actually an Add node, then this will call
     * IRVisitor::visit(const Add *) */
    void accept(IRVisitor *v) const {
        ptr->accept(v);
    }

    /** Downcast this ir node to its actual type (e.g. Add, or
     * Select). This returns NULL if the node is not of the requested
     * type. Example usage:
     * 
     * if (const Add *add = node->as<Add>()) {
     *   // This is an add node
     * }
     */
    template<typename T> const T *as() const {
        if (ptr->type_info() == &T::_type_info)
            return (const T *)ptr;
        return NULL;
    }
};

/** Integer constants */
struct IntImm : public ExprNode<IntImm> {
    int value;

    static IntImm *make(int value) {
        if (value >= -8 && value <= 8) return small_int_cache + value + 8;
        IntImm *node = new IntImm;
        node->type = Int(32);
        node->value = value;
        return node;
    }

private:
    /** ints from -8 to 8 */
    static IntImm small_int_cache[17];
};

/** Floating point constants */
struct FloatImm : public ExprNode<FloatImm> {
    float value;

    static FloatImm *make(float value) {
        FloatImm *node = new FloatImm;
        node->type = Float(32);
        node->value = value;
        return node;
    }
};

}

/** A fragment of Halide syntax. It's implemented as reference-counted
 * handle to a concrete expression node, but it's immutable, so you
 * can treat it as a value type. */
struct Expr : public Internal::IRHandle {
    /** Make an undefined expression */
    Expr() : Internal::IRHandle() {}        

    /** Make an expression from a concrete expression node pointer (e.g. Add) */
    Expr(const Internal::BaseExprNode *n) : IRHandle(n) {}

    
    /** Make an expression representing a const 32-bit int (i.e. an IntImm) */
    EXPORT Expr(int x) : IRHandle(Internal::IntImm::make(x)) {
    }

    /** Make an expression representing a const 32-bit float (i.e. a FloatImm) */
    EXPORT Expr(float x) : IRHandle(Internal::FloatImm::make(x)) {
    }   
    
    EXPORT Expr(double); //LH

    /** Get the type of this expression node */
    Type type() const {
        return ((Internal::BaseExprNode *)ptr)->type;
    }

    /** This lets you use an Expr as a key in a map of the form
     * map<Expr, Foo, Expr::Compare> */
    struct Compare {
        bool operator()(const Expr &a, const Expr &b) const {
            return a.ptr < b.ptr;
        }
    };
};    

}

// Now that we've defined an Expr, we can include Parameter.h
#include "Parameter.h"
// Include clamp expression nodes.
#include "Clamp.h"

namespace Halide {
namespace Internal {

/** A reference-counted handle to a statement node. */
struct Stmt : public IRHandle {
    Stmt() : IRHandle() {}
    Stmt(const BaseStmtNode *n) : IRHandle(n) {}

    /** This lets you use a Stmt as a key in a map of the form
     * map<Stmt, Foo, Stmt::Compare> */
    struct Compare {
        bool operator()(const Stmt &a, const Stmt &b) const {
            return a.ptr < b.ptr;
        }
    };
};

/** The actual IR nodes begin here. Remember that all the Expr
 * nodes also have a public "type" property */
 
}

namespace Internal {

/** Cast a node from one type to another */
struct Cast : public ExprNode<Cast> {
    Expr value;

    static Expr make(Type t, Expr v) {
        assert(v.defined() && "Cast of undefined");

        Cast *node = new Cast;
        node->type = t;
        node->value = v;
        return node;
    }
};

//LH
/** Bit-wise AND */
struct BitAnd : public ExprNode<BitAnd> {
    Expr a, b;

    BitAnd(Expr _a, Expr _b) : ExprNode<BitAnd>(_a.type()), a(_a), b(_b) {
        assert(a.defined() && "BitAnd of undefined");
        assert(b.defined() && "BitAnd of undefined");
        assert((a.type().is_int() || a.type().is_uint()) && "lhs of BitAnd is not an integer type");
        assert((b.type().is_int() || b.type().is_uint()) && "rhs of BitAnd is not an integer type");
    }
};

//LH
/** Bit-wise OR */
struct BitOr : public ExprNode<BitOr> {
    Expr a, b;

    BitOr(Expr _a, Expr _b) : ExprNode<BitOr>(_a.type()), a(_a), b(_b) {
        assert(a.defined() && "BitOr of undefined");
        assert(b.defined() && "BitOr of undefined");
        assert((a.type().is_int() || a.type().is_uint()) && "lhs of BitOr is not an integer type");
        assert((b.type().is_int() || b.type().is_uint()) && "rhs of BitOr is not an integer type");
    }
};

//LH
/** Bit-wise XOR */
struct BitXor : public ExprNode<BitXor> {
    Expr a, b;

    BitXor(Expr _a, Expr _b) : ExprNode<BitXor>(_a.type()), a(_a), b(_b) {
        assert(a.defined() && "BitXor of undefined");
        assert(b.defined() && "BitXor of undefined");
        assert((a.type().is_int() || a.type().is_uint()) && "lhs of BitXor is not an integer type");
        assert((b.type().is_int() || b.type().is_uint()) && "rhs of BitXor is not an integer type");
    }
};

//LH
/** Fill integer with the sign bit */
struct SignFill : public ExprNode<SignFill> {
    Expr value;

    SignFill(Expr _value) : ExprNode<SignFill>(_value.type()), value(_value) {
        assert(value.defined() && "SignFill of undefined");
        assert((value.type().is_int() || value.type().is_uint()) && "parameter of SignFill is not an integer type");
    }
};

/** The sum of two expressions */
struct Add : public ExprNode<Add> {
    Expr a, b;

    static Expr make(Expr a, Expr b) {
        assert_defined_same_type("Add", a, b);
        //assert(a.defined() && "Add of undefined");
        //assert(b.defined() && "Add of undefined");
        //assert(a.type() == b.type() && "Add of mismatched types");

        Add *node = new Add;
        node->type = a.type();
        node->a = a;
        node->b = b;
        return node;
    }
};

/** The difference of two expressions */
struct Sub : public ExprNode<Sub> {
    Expr a, b;

    static Expr make(Expr a, Expr b) {
        assert_defined_same_type("Sub", a, b);
        //assert(a.defined() && "Sub of undefined");
        //assert(b.defined() && "Sub of undefined");
        //assert(a.type() == b.type() && "Sub of mismatched types");

        Sub *node = new Sub;
        node->type = a.type();
        node->a = a;
        node->b = b;
        return node;
    }
};

/** The product of two expressions */
struct Mul : public ExprNode<Mul> {
    Expr a, b;

    static Expr make(Expr a, Expr b) {
        assert_defined_same_type("Mul", a, b);
        //assert(a.defined() && "Mul of undefined");
        ///assert(b.defined() && "Mul of undefined");
        /assert(a.type() == b.type() && "Mul of mismatched types");

        Mul *node = new Mul;
        node->type = a.type();
        node->a = a;
        node->b = b;
        return node;
    }        
};

/** The ratio of two expressions */
struct Div : public ExprNode<Div> {
    Expr a, b;

    static Expr make(Expr a, Expr b) {
        assert_defined_same_type("Div", a, b);
        //assert(a.defined() && "Div of undefined");
        //assert(b.defined() && "Div of undefined");
        //assert(a.type() == b.type() && "Div of mismatched types");

        Div *node = new Div;
        node->type = a.type();
        node->a = a;
        node->b = b;
        return node;
    }
};

/** The remainder of a / b. Mostly equivalent to '%' in C, except that
 * the result here is always positive (//LH: actually, not quite correct.
 * It has the same sign as the modulus b). For floats, this is equivalent
 * to calling fmod. */
struct Mod : public ExprNode<Mod> { 
    Expr a, b;

    static Expr make(Expr a, Expr b) {
        assert_defined_same_type("Mod", a, b);
        //assert(a.defined() && "Mod of undefined");
        //assert(b.defined() && "Mod of undefined");
        //assert(a.type() == b.type() && "Mod of mismatched types");

        Mod *node = new Mod;
        node->type = a.type();
        node->a = a;
        node->b = b;
        return node;
    }
};

/** The lesser of two values. */
struct Min : public ExprNode<Min> {
    Expr a, b;

    static Expr make(Expr a, Expr b) {
        assert_defined_same_type("Min", a, b);
        //assert(a.defined() && "Min of undefined");
        ///assert(b.defined() && "Min of undefined");
        /assert(a.type() == b.type() && "Min of mismatched types");

        Min *node = new Min;
        node->type = a.type();
        node->a = a;
        node->b = b;
        return node;        
    }
};

/** The greater of two values */
struct Max : public ExprNode<Max> {
    Expr a, b;

    static Expr make(Expr a, Expr b) {
        assert_defined_same_type("Max", a, b);
        //assert(a.defined() && "Max of undefined");
        //assert(b.defined() && "Max of undefined");
        //assert(a.type() == b.type() && "Max of mismatched types");

        Max *node = new Max;
        node->type = a.type();
        node->a = a;
        node->b = b;
        return node;        
    }
};

/** Is the first expression equal to the second */
struct EQ : public ExprNode<EQ> {
    Expr a, b;

    static Expr make(Expr a, Expr b) {
        assert(a.defined() && "EQ of undefined");
        assert(b.defined() && "EQ of undefined");
        assert(a.type() == b.type() && "EQ of mismatched types");

        EQ *node = new EQ;
        node->type = Bool(a.type().width);
        node->a = a;
        node->b = b;
        return node;
    }
};

/** Is the first expression not equal to the second */
struct NE : public ExprNode<NE> {
    Expr a, b;

    static Expr make(Expr a, Expr b) {
        assert(a.defined() && "NE of undefined");
        assert(b.defined() && "NE of undefined");
        assert(a.type() == b.type() && "NE of mismatched types");

        NE *node = new NE;
        node->type = Bool(a.type().width);
        node->a = a;
        node->b = b;
        return node;
    }
};

/** Is the first expression less than the second. */
struct LT : public ExprNode<LT> {
    Expr a, b;

    static Expr make(Expr a, Expr b) {
        assert(a.defined() && "LT of undefined");
        assert(b.defined() && "LT of undefined");
        assert(a.type() == b.type() && "LT of mismatched types");

        LT *node = new LT;
        node->type = Bool(a.type().width);
        node->a = a;
        node->b = b;
        return node;
    }
};

/** Is the first expression less than or equal to the second. */
struct LE : public ExprNode<LE> {
    Expr a, b;

    static Expr make(Expr a, Expr b) {
        assert(a.defined() && "LE of undefined");
        assert(b.defined() && "LE of undefined");
        assert(a.type() == b.type() && "LE of mismatched types");

        LE *node = new LE;
        node->type = Bool(a.type().width);
        node->a = a;
        node->b = b;
        return node;
    }
};

/** Is the first expression greater than the second. */
struct GT : public ExprNode<GT> {
    Expr a, b;

    static Expr make(Expr a, Expr b) {
        assert(a.defined() && "GT of undefined");
        assert(b.defined() && "GT of undefined");
        assert(a.type() == b.type() && "GT of mismatched types");

        GT *node = new GT;
        node->type = Bool(a.type().width);
        node->a = a;
        node->b = b;
        return node;
    }
};

/** Is the first expression greater than or equal to the second. */
struct GE : public ExprNode<GE> {
    Expr a, b;

    static Expr make(Expr a, Expr b) {
        assert(a.defined() && "GE of undefined");
        assert(b.defined() && "GE of undefined");
        assert(a.type() == b.type() && "GE of mismatched types");

        GE *node = new GE;
        node->type = Bool(a.type().width);
        node->a = a;
        node->b = b;
        return node;
    }
};

/** Logical and - are both expressions true */
struct And : public ExprNode<And> {
    Expr a, b;

    static Expr make(Expr a, Expr b) {
        assert(a.defined() && "And of undefined");
        assert(b.defined() && "And of undefined");
        assert(a.type().is_bool() && "lhs of And is not a bool");
        assert(b.type().is_bool() && "rhs of And is not a bool");

        And *node = new And;
        node->type = Bool(a.type().width);
        node->a = a;
        node->b = b;
        return node;
    }
};

/** Logical or - is at least one of the expression true */
struct Or : public ExprNode<Or> {
    Expr a, b;

    static Expr make(Expr a, Expr b) {
        assert(a.defined() && "Or of undefined");
        assert(b.defined() && "Or of undefined");
        assert(a.type().is_bool() && "lhs of Or is not a bool");
        assert(b.type().is_bool() && "rhs of Or is not a bool");

        Or *node = new Or;
        node->type = Bool(a.type().width);
        node->a = a;
        node->b = b;
        return node;
    }
};

/** Logical not - true if the expression false */
struct Not : public ExprNode<Not> {
    Expr a;

    static Expr make(Expr a) {
        assert(a.defined() && "Not of undefined");
        assert(a.type().is_bool() && "argument of Not is not a bool");

        Not *node = new Not;
        node->type = Bool(a.type().width);
        node->a = a;
        return node;
    }
};

/** A ternary operator. Evalutes 'true_value' and 'false_value',
 * then selects between them based on 'condition'. Equivalent to
 * the ternary operator in C. */
struct Select : public ExprNode<Select> {
    Expr condition, true_value, false_value;

    static Expr make(Expr condition, Expr true_value, Expr false_value) {
        assert(condition.defined() && "Select of undefined");
        assert(true_value.defined() && "Select of undefined");
        assert(false_value.defined() && "Select of undefined");
        assert(condition.type().is_bool() && "First argument to Select is not a bool");
        assert(false_value.type() == true_value.type() && "Select of mismatched types");
        assert((condition.type().is_scalar() ||
                condition.type().width == true_value.type().width) &&
               "In Select, vector width of condition must either be 1, or equal to vector width of arguments");

        Select *node = new Select;
        node->type = true_value.type();
        node->condition = condition;
        node->true_value = true_value;
        node->false_value = false_value;
        return node;
    }
};

/** Load a value from a named buffer. The buffer is treated as an
 * array of the 'type' of this Load node. That is, the buffer has
 * no inherent type. */
struct Load : public ExprNode<Load> {
    std::string name;
    Expr index;

    // If it's a load from an image argument or compiled-in constant
    // image, this will point to that
    Buffer image;

    // If it's a load from an image parameter, this points to that
    Parameter param;

    static Expr make(Type type, std::string name, Expr index, Buffer image, Parameter param) {
        assert(index.defined() && "Load of undefined");
        assert(type.width == index.type().width && "Vector width of Load must match vector width of index");

        Load *node = new Load;
        node->type = type;
        node->name = name;
        node->index = index;
        node->image = image;
        node->param = param;
        return node;
    }
};

/** A linear ramp vector node. This is vector with 'width' elements,
 * where element i is 'base' + i*'stride'. This is a convenient way to
 * pass around vectors without busting them up into individual
 * elements. E.g. a dense vector load from a buffer can use a ramp
 * node with stride 1 as the index. */
struct Ramp : public ExprNode<Ramp> {
    Expr base, stride;
    int width;

    static Expr make(Expr base, Expr stride, int width) {
        assert(base.defined() && "Ramp of undefined");
        assert(stride.defined() && "Ramp of undefined");
        assert(base.type().is_scalar() && "Ramp with vector base");
        assert(stride.type().is_scalar() && "Ramp with vector stride");
        assert(width > 1 && "Ramp of width <= 1");
        assert(stride.type() == base.type() && "Ramp of mismatched types");
        
        Ramp *node = new Ramp;
        node->type = base.type().vector_of(width);
        node->base = base;
        node->stride = stride;
        node->width = width;
        return node;
    }
};

/** A vector with 'width' elements, in which every element is
 * 'value'. This is a special case of the ramp node above, in which
 * the stride is zero. */
struct Broadcast : public ExprNode<Broadcast> {
    Expr value;
    int width;
        
    static Expr make(Expr value, int width) {
        assert(value.defined() && "Broadcast of undefined");
        assert(value.type().is_scalar() && "Broadcast of vector");
        assert(width > 1 && "Broadcast of width <= 1");            

        Broadcast *node = new Broadcast;
        node->type = value.type().vector_of(width);
        node->value = value;
        node->width = width;
        return node;
    }
};

/** A let expression, like you might find in a functional
 * language. Within the expression \ref body, instances of the Var
 * node \ref name refer to \ref value. */
struct Let : public ExprNode<Let> {
    std::string name;
    Expr value, body;

    static Expr make(std::string name, Expr value, Expr body) {
        assert(value.defined() && "Let of undefined");
        assert(body.defined() && "Let of undefined");

        Let *node = new Let;
        node->type = value.type();
        node->name = name;
        node->value = value;
        node->body = body;        
        return node;
    }
};

/** The statement form of a let node. Within the statement 'body',
 * instances of the Var named 'name' refer to 'value' */
struct LetStmt : public StmtNode<LetStmt> {
    std::string name;
    Expr value;
    Stmt body;

    static Stmt make(std::string name, Expr value, Stmt body) {
        assert(value.defined() && "Let of undefined");
        assert(body.defined() && "Let of undefined");

        LetStmt *node = new LetStmt;
        node->name = name;
        node->value = value;
        node->body = body;        
        return node;
    }
};

/** Used largely for debugging and tracing. Dumps the 'prefix'
 * string and the args to stdout. */
struct PrintStmt : public StmtNode<PrintStmt> {
    std::string prefix;
    std::vector<Expr> args;

    static Stmt make(std::string prefix, const std::vector<Expr> &args) {
        for (size_t i = 0; i < args.size(); i++) {
            assert(args[i].defined() && "PrintStmt of undefined");
        }
        
        PrintStmt *node = new PrintStmt;
        node->prefix = prefix;
        node->args = args;
        return node;
    }
};

/** If the 'condition' is false, then bail out printing the
 * 'message' to stderr */
struct AssertStmt : public StmtNode<AssertStmt> {
    // if condition then val else error out with message
    Expr condition;
    std::string message;

    static Stmt make(Expr condition, std::string message) {
        assert(condition.defined() && "AssertStmt of undefined");
        assert(condition.type().is_scalar() && "AssertStmt of vector");

        AssertStmt *node = new AssertStmt;
        node->condition = condition;
        node->message = message;
        return node;
    }
};

/** This node is a helpful annotation to do with permissions. The
 * three child statements happen in order. In the 'produce'
 * statement 'buffer' is write-only. In 'update' it is
 * read-write. In 'consume' it is read-only. The 'update' node is
 * often NULL. (check update.defined() to find out). None of this
 * is actually enforced, the node is purely for informative
 * purposes to help out our analysis during lowering. */ 
struct Pipeline : public StmtNode<Pipeline> {
    std::string name;
    Stmt produce, update, consume;

    static Stmt make(std::string name, Stmt produce, Stmt update, Stmt consume) {
        assert(produce.defined() && "Pipeline of undefined");
        // update is allowed to be null
        assert(consume.defined() && "Pipeline of undefined");

        Pipeline *node = new Pipeline;
        node->name = name;
        node->produce = produce;
        node->update = update;
        node->consume = consume;
        return node;
    }
};

// end namespace Internal
}
}

#include "IntRange.h"

namespace Halide {
namespace Internal {

/** Information for loop partitioning from the .partition schedule.
 * It is stored in the Dim portion of the Schedule, and later into the For loops. */
    struct PartitionInfo {
        /** One option is for the user to partition the main loop manually.
         * Specify an InfInterval for the loop.  The bounds can be expressions.
         * If not used, the expressions will be undefined. */
        Halide::InfInterval interval;
        /** Boolean options translate to tristate variables internally because they can
         * be undefined. */
        enum TriState { Undefined, No, Yes };
        /** Record the auto_partition option for this variable. */
        TriState auto_partition;
      
        /** Record the status of For loops, mainly for debugging and optimisation. */
        enum LoopStatus { Ordinary, Before, Main, After };
        LoopStatus status;
        
        PartitionInfo(bool do_partition) {
            auto_partition = do_partition ? Yes : No;
            status = Ordinary;
        }
        PartitionInfo(InfInterval _interval) { 
            interval = _interval; 
            auto_partition = Undefined; 
            status = Ordinary;
        }
        PartitionInfo(TriState _auto_partition) { 
            assert(_auto_partition == Undefined || _auto_partition == Yes || _auto_partition == No);
            auto_partition = _auto_partition; 
            status = Ordinary;
        }
        PartitionInfo() { 
            auto_partition = Undefined; 
            status = Ordinary;
        }
        
        const bool defined() const;
        const bool interval_defined() const;
    };
        
/** A for loop. Execute the 'body' statement for all values of the
 * variable 'name' from 'min' to 'min + extent'. There are four
 * types of For nodes. A 'Serial' for loop is a conventional
 * one. In a 'Parallel' for loop, each iteration of the loop
 * happens in parallel or in some unspecified order. In a
 * 'Vectorized' for loop, each iteration maps to one SIMD lane,
 * and the whole loop is executed in one shot. For this case,
 * 'extent' must be some small integer constant (probably 4, 8, or
 * 16). An 'Unrolled' for loop compiles to a completely unrolled
 * version of the loop. Each iteration becomes its own
 * statement. Again in this case, 'extent' should be a small
 * integer constant. */
struct For : public StmtNode<For> {
    std::string name;
    Expr min, extent;
    typedef enum {Serial, Parallel, Vectorized, Unrolled} ForType;
    ForType for_type;
    PartitionInfo partition;
    Stmt body;

# if 0
    /** Constructor for building a For loop with partition schedule information included. */
    For(std::string n, Expr m, Expr e, ForType f, const PartitionInfo &p, Stmt b) :
        name(n), min(m), extent(e), for_type(f), partition(p), body(b) {
        assert(min.defined() && "For of undefined");
        assert(extent.defined() && "For of undefined");
        assert(min.type().is_scalar() && "For with vector min");
        assert(extent.type().is_scalar() && "For with vector extent");
        assert(body.defined() && "For of undefined");
    }
    
    /** Convenience constructor that inherits information from an existing For structure.
     * name, for_type and partition are inherited. Use this in mutators unless you need to
     * modify the excluded parameters.  */
    For(const For *oldloop, Expr m, Expr e, Stmt b) :
        min(m), extent(e), body(b) {
        assert(oldloop && "Null pointer passed to For");
        name = oldloop->name;
        for_type = oldloop->for_type;
        partition = oldloop->partition;
        assert(min.defined() && "For of undefined");
        assert(extent.defined() && "For of undefined");
        assert(min.type().is_scalar() && "For with vector min");
        assert(extent.type().is_scalar() && "For with vector extent");
        assert(body.defined() && "For of undefined");
    }
    
    /** Convenience constructor for building sample code. Do NOT use this in mutators as you will lose information. */
    For(std::string n, Expr m, Expr e, ForType f, Stmt b) :
        name(n), min(m), extent(e), for_type(f), partition(PartitionInfo()), body(b) {
# endif
    static Stmt make(std::string name, Expr min, Expr extent, ForType for_type, Stmt body) {
        assert(min.defined() && "For of undefined");
        assert(extent.defined() && "For of undefined");
        assert(min.type().is_scalar() && "For with vector min");
        assert(extent.type().is_scalar() && "For with vector extent");
        assert(body.defined() && "For of undefined");

        For *node = new For;
        node->name = name;
        node->min = min;
        node->extent = extent;
        node->for_type = for_type;
        node->body = body;
        return node;
    }
    
};

/** Store a 'value' to the buffer called 'name' at a given
 * 'index'. The buffer is interpreted as an array of the same type as
 * 'value'. */
struct Store : public StmtNode<Store> {
    std::string name;
    Expr value, index;

    static Stmt make(std::string name, Expr value, Expr index) {
        assert(value.defined() && "Store of undefined");
        assert(index.defined() && "Store of undefined");

        Store *node = new Store;
        node->name = name;
        node->value = value;
        node->index = index;
        return node;
    }
};

/** This defines the value of a function at a multi-dimensional
 * location. You should think of it as a store to a
 * multi-dimensional array. It gets lowered to a conventional
 * Store node. */
struct Provide : public StmtNode<Provide> {
    std::string name;
    Expr value;
    std::vector<Expr> args;

    static Stmt make(std::string name, Expr value, const std::vector<Expr> &args) {
        assert(value.defined() && "Provide of undefined");
        for (size_t i = 0; i < args.size(); i++) {
            assert(args[i].defined() && "Provide of undefined");
        }

        Provide *node = new Provide;
        node->name = name;
        node->value = value;
        node->args = args;
        return node;
    }
};

/** Allocate a scratch area called with the given name, type, and
 * size. The buffer lives for at most the duration of the body
 * statement, within which it is freed. It is an error for an allocate
 * node not to contain a free node of the same buffer. */
struct Allocate : public StmtNode<Allocate> {
    std::string name;
    Type type;
    Expr size;
    Stmt body;

    static Stmt make(std::string name, Type type, Expr size, Stmt body) {
        assert(size.defined() && "Allocate of undefined");
        assert(body.defined() && "Allocate of undefined");
        assert(size.type().is_scalar() == 1 && "Allocate of vector size");

        Allocate *node = new Allocate;
        node->name = name;
        node->type = type;
        node->size = size;
        node->body = body;
        return node;
    }
};

/** Free the resources associated with the given buffer. */
struct Free : public StmtNode<Free> {
    std::string name;
    
    static Stmt make(std::string name) {
        Free *node = new Free;
        node->name = name;
        return node;
    }
};

# if 0
/** A single-dimensional span. Includes all numbers between min and
 * (min + extent - 1) */
struct Range {
    Expr min, extent;
    Range() {}
    Range(Expr min, Expr extent) : min(min), extent(extent) {
        assert(min.type() == extent.type() && "Region min and extent must have same type");
    }
};
# endif
#include "IntRange.h"

/** A multi-dimensional box. The outer product of the elements */
typedef std::vector<Range> Region;   

/** Allocate a multi-dimensional buffer of the given type and
 * size. Create some scratch memory that will back the function 'name'
 * over the range specified in 'bounds'. The bounds are a vector of
 * (min, extent) pairs for each dimension. */
struct Realize : public StmtNode<Realize> {
    std::string name;
    Type type;
    Region bounds;
    Stmt body;

    static Stmt make(std::string name, Type type, const Region &bounds, Stmt body) {
        for (size_t i = 0; i < bounds.size(); i++) {
            assert(bounds[i].min.defined() && "Realize of undefined");
            assert(bounds[i].extent.defined() && "Realize of undefined");
            assert(bounds[i].min.type().is_scalar() && "Realize of vector size");
            assert(bounds[i].extent.type().is_scalar() && "Realize of vector size");
        }
        assert(body.defined() && "Realize of undefined");

        Realize *node = new Realize;
        node->name = name;
        node->type = type;
        node->bounds = bounds;
        node->body = body;
        return node;
    }
};

/** A sequence of statements to be executed in-order. 'rest' may be
 * NULL. Used rest.defined() to find out. */
struct Block : public StmtNode<Block> {
    Stmt first, rest;
        
    static Stmt make(Stmt first, Stmt rest) {
        assert(first.defined() && "Block of undefined");
        // rest is allowed to be null

        Block *node = new Block;
        node->first = first;
        node->rest = rest;
        return node;
    }
};

}
}

//LH Include the definition of a Domain
#include "DomainInference.h"

// Now that we've defined an Expr and ForType, we can include the definition of a function
#include "Function.h"

// And the definition of a reduction domain
#include "Reduction.h"

namespace Halide {
namespace Internal {

/** A function call. This can represent a call to some extern
 * function (like sin), but it's also our multi-dimensional
 * version of a Load, so it can be a load from an input image, or
 * a call to another halide function. The latter two types of call
 * nodes don't survive all the way down to code generation - the
 * lowering process converts them to Load nodes. */
struct Call : public ExprNode<Call> {
    std::string name;
    std::vector<Expr> args;
    typedef enum {Image, Extern, Halide} CallType;
    CallType call_type;

    // If it's a call to another halide function, this call node
    // holds onto a pointer to that function
    Function func;

    // If it's a call to an image, this call nodes hold a
    // pointer to that image's buffer
    Buffer image;

    // If it's a call to an image parameter, this call nodes holds a
    // pointer to that
    Parameter param;

    static Expr make(Type type, std::string name, const std::vector<Expr> &args, CallType call_type, 
                            Function func, Buffer image, Parameter param) { 
        for (size_t i = 0; i < args.size(); i++) {
            assert(args[i].defined() && "Call of undefined");
        }
        if (call_type == Halide) {
            assert(func.value().defined() && "Call to undefined halide function");
            assert(args.size() <= func.args().size() && "Call node with too many arguments.");
        } else if (call_type == Image) {
            assert((param.defined() || image.defined()) && "Call node to undefined image");
        }

        Call *node = new Call;
        node->type = type;
        node->name = name;
        node->args = args;
        node->call_type = call_type;
        node->func = func;
        node->image = image;
        node->param = param;
        return node;
    }

    /** Convenience constructor for calls to externally defined functions */
    static Expr make(Type type, std::string name, const std::vector<Expr> &args) {
        return make(type, name, args, Extern, Function(), Buffer(), Parameter());
    }

    /** Convenience constructor for calls to other halide functions */
    static Expr make(Function func, const std::vector<Expr> &args) {
        assert(func.value().defined() && "Call to undefined halide function");
        return make(func.value().type(), func.name(), args, Halide, func, Buffer(), Parameter());
    }

    /** Convenience constructor for loads from concrete images */
    static Expr make(Buffer image, const std::vector<Expr> &args) {
        return make(image.type(), image.name(), args, Image, Function(), image, Parameter());
    }

    /** Convenience constructor for loads from images parameters */
    static Expr make(Parameter param, const std::vector<Expr> &args) {
        return make(param.type(), param.name(), args, Image, Function(), Buffer(), param);
    }
};

/** A named variable. Might be a loop variable, function argument,
 * parameter, reduction variable, or something defined by a Let or
 * LetStmt node. */
struct Variable : public ExprNode<Variable> {
    std::string name;

    /** References to scalar parameters, or to the dimensions of buffer
     * parameters hang onto those expressions */
    Parameter param;

    /** Reduction variables hang onto their domains */
    ReductionDomain reduction_domain;

    static Expr make(Type type, std::string name) {
        return make(type, name, Parameter(), ReductionDomain());
    }

    static Expr make(Type type, std::string name, Parameter param) {
        return make(type, name, param, ReductionDomain());
    }

    static Expr make(Type type, std::string name, ReductionDomain reduction_domain) {
        return make(type, name, Parameter(), reduction_domain);
    }

    static Expr make(Type type, std::string name, Parameter param, ReductionDomain reduction_domain) {
        Variable *node = new Variable;
        node->type = type;
        node->name = name;
        node->param = param;
        node->reduction_domain = reduction_domain;
        return node;
    }

};

/** Special nodes that are for use by the solver.
 * Solve represents something to be solved, or a solution.
 * TargetVar represents a target variable to be solved for in the enclosed scope.
 */
struct Solve : public ExprNode<Solve> {
    Expr body;
    std::vector<InfInterval> v;
    
    // Solve over a vector of InfInterval.
    Solve(Expr _e, std::vector<InfInterval> _v) : ExprNode<Solve>(_e.type()), body(_e), v(_v) {}
    // Solve over one InfInterval
    Solve(Expr _e, InfInterval _i) : ExprNode<Solve>(_e.type()), body(_e), v(vec(_i)) {}
    // Solve over two InfInterval
    Solve(Expr _e, InfInterval _i, InfInterval _j) : ExprNode<Solve>(_e.type()), body(_e), v(vec(_i, _j)) {}
};

struct TargetVar : public ExprNode<TargetVar> {
    std::string name;
    Expr body;
    Expr source; // Not a child node - records the source expression
    
    TargetVar(std::string _v, Expr _e, Expr _source) : ExprNode<TargetVar>(_e.type()), name(_v), body(_e), source(_source) {}
    TargetVar(const TargetVar *op, Expr _e) : ExprNode<TargetVar>(_e.type()), name(op->name), body(_e), source(op->source) {}
};

struct StmtTargetVar : public StmtNode<StmtTargetVar> {
    std::string name;
    Stmt body;
    Stmt source; // Not a child node - records the source statement
    
    StmtTargetVar(std::string _v, Stmt _s, Stmt _source) : name(_v), body(_s), source(_source) {}
    // Constructor for use by mutators that want to mutate the Stmt body.
    StmtTargetVar(const StmtTargetVar *op, Stmt _s) : name(op->name), body(_s), source(op->source) {}
};

/** Infinity node is useful for interval analysis and solver.  It represents an undefined
 * minimum of an interval as negative infinity, and an undefined maximum as positive infinity.
 * Simplify is made able to simplify expressions involving infinity nodes.
 * The type of Infinity is specified at construction.
 */
struct Infinity : public ExprNode<Infinity> {
    int count; // Count of infinity. >0 means +ve infinity, <0 means negative infinity.
    
    Infinity(Type t, int _c) : ExprNode<Infinity>(t), count(_c) {}
    
    // Convenience constructor when you dont know what type to use.  Be careful.
    Infinity(int _c) : ExprNode<Infinity>(Int(32)), count(_c) {}
};

}
}

#endif
