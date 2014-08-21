#include "HalideFeatures.h"
#include "IR.h"
#include "IRPrinter.h"
#include <iostream>

namespace Halide {
namespace Internal {

namespace {

IntImm make_immortal_int(int x) {
    IntImm i;
    i.ref_count.increment();
    i.type = Int(32);
    i.value = x;
    return i;
}

}

IntImm IntImm::small_int_cache[] = {make_immortal_int(-8), 
                                    make_immortal_int(-7), 
                                    make_immortal_int(-6), 
                                    make_immortal_int(-5), 
                                    make_immortal_int(-4), 
                                    make_immortal_int(-3), 
                                    make_immortal_int(-2), 
                                    make_immortal_int(-1), 
                                    make_immortal_int(0), 
                                    make_immortal_int(1), 
                                    make_immortal_int(2), 
                                    make_immortal_int(3), 
                                    make_immortal_int(4), 
                                    make_immortal_int(5), 
                                    make_immortal_int(6), 
                                    make_immortal_int(7), 
                                    make_immortal_int(8)};
                            


template<> EXPORT IRNodeType ExprNode<IntImm>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<FloatImm>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<Cast>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<Variable>::_type_info = {};
#ifdef HALIDE_CLAMP_NODE
template<> EXPORT IRNodeType ExprNode<Clamp>::_type_info = {};
#endif
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
template<> EXPORT IRNodeType StmtNode<Free>::_type_info = {};
template<> EXPORT IRNodeType StmtNode<Realize>::_type_info = {};
template<> EXPORT IRNodeType StmtNode<Block>::_type_info = {};

template<> EXPORT IRNodeType ExprNode<Solve>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<TargetVar>::_type_info = {};
template<> EXPORT IRNodeType StmtNode<StmtTargetVar>::_type_info = {};
template<> EXPORT IRNodeType ExprNode<Infinity>::_type_info = {};
//template<> EXPORT IRNodeType ExprNode<ExprInterval>::_type_info = {};

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

// Loop Split information is defined if auto_split has been set or if
// a loop split interval has been specified.  Since the interval is a DomInterval,
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
