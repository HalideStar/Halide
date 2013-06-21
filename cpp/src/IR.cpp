#include "IR.h"
#include "IRPrinter.h"
#include <iostream>

namespace Halide {

Expr::Expr(int x) : Internal::IRHandle(new Internal::IntImm(x)) {
}

Expr::Expr(float x) : Internal::IRHandle(new Internal::FloatImm(x)) {
}

Expr::Expr(double x) : Internal::IRHandle(new Internal::FloatImm((float)x)) {
}

namespace Internal {

template<> EXPORT IRNodeType ExprNode<IntImm>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<FloatImm>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<Cast>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<Variable>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<BitAnd>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<BitOr>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<BitXor>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<SignFill>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<Clamp>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<Add>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<Sub>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<Mul>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<Div>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<Mod>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<Min>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<Max>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<EQ>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<NE>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<LT>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<LE>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<GT>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<GE>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<And>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<Or>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<Not>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<Select>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<Load>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<Ramp>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<Broadcast>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<Call>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<Let>::_type_info = {};
template<> EXPORT IRNodeType StmtNode<LetStmt>::_type_info = {};
template<> EXPORT IRNodeType StmtNode<PrintStmt>::_type_info = {};
template<> EXPORT IRNodeType StmtNode<AssertStmt>::_type_info = {};
template<> EXPORT IRNodeType StmtNode<Pipeline>::_type_info = {};
template<> EXPORT IRNodeType StmtNode<For>::_type_info = {};
template<> EXPORT IRNodeType StmtNode<Store>::_type_info = {};
template<> EXPORT IRNodeType StmtNode<Provide>::_type_info = {};
template<> EXPORT IRNodeType StmtNode<Allocate>::_type_info = {};
template<> EXPORT IRNodeType StmtNode<Realize>::_type_info = {};
template<> EXPORT IRNodeType StmtNode<Block>::_type_info = {};

template<> EXPORT IRNodeType ExprNode<Solve>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<TargetVar>::_type_info = {};
template<> EXPORT IRNodeType StmtNode<StmtTargetVar>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<Infinity>::_type_info = {};
//template<> EXPORT IRNodeType ExprNode<ExprInterval>::_type_info = {};

template<>
EXPORT RefCount &ref_count<IRNode>(const IRNode *n) {return n->ref_count;}

template<>
EXPORT void destroy<IRNode>(const IRNode *n) {delete n;}

/** Ensure that two operands are both defined and of the same type */
void assert_defined_same_type(std::string opname, Expr a, Expr b) {
    if (! a.defined() || ! b.defined()) {
        std::cerr << opname << "(" << a << ", " << b << ") has undefined operand" << std::endl;
        assert(0 && "Undefined operand");
    }
    if (a.type() == b.type()) return;
    std::cerr << opname << "(" << a << ", " << b << ") has mismatched types " 
        << a.type() << " and " << b.type() << std::endl;
    assert(0 && "Mismatched types");
}

/** Ensure that two operands are of the same type if they are both defined */
void assert_same_type(std::string opname, Expr a, Expr b) {
    if (! a.defined() || ! b.defined()) return;
    if (a.type() == b.type()) return;
    std::cerr << opname << "(" << a << ", " << b << ") has mismatched types " 
        << a.type() << " and " << b.type() << std::endl;
    assert(0 && "Mismatched types");
}

// Partition information is defined if auto_partition has been set or if
// a partition interval has been specified.  Since the interval is an Ival,
// it should not contain undefined() expressions, but Infinity means undefined
// in this case.
const bool LoopSplitInfo::defined() const { 
    return auto_split != Undefined || 
           (interval.min.defined() && interval.max.defined() && 
            ! interval.min.as<Infinity>() && ! interval.max.as<Infinity>()); 
}

const bool LoopSplitInfo::interval_defined() const { 
    return (interval.min.defined() && interval.max.defined() && 
            ! interval.min.as<Infinity>() && ! interval.max.as<Infinity>()); 
}

const bool LoopSplitInfo::may_be_split() const { 
    return auto_split != No || interval_defined(); 
}



}
}
