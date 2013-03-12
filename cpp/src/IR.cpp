#include "IR.h"

namespace Halide {

Expr::Expr(int x) : Internal::IRHandle(new Internal::IntImm(x)) {
}

Expr::Expr(float x) : Internal::IRHandle(new Internal::FloatImm(x)) {
}

namespace Internal {

template<> IRNodeType ExprNode<IntImm>::_type_info = {};
template<> IRNodeType ExprNode<FloatImm>::_type_info = {};
template<> IRNodeType ExprNode<Cast>::_type_info = {};
template<> IRNodeType ExprNode<Variable>::_type_info = {};
template<> IRNodeType ExprNode<BitAnd>::_type_info = {}; //LH
template<> IRNodeType ExprNode<BitOr>::_type_info = {}; //LH
template<> IRNodeType ExprNode<BitXor>::_type_info = {}; //LH
template<> IRNodeType ExprNode<SignFill>::_type_info = {}; //LH
template<> IRNodeType ExprNode<Clamp>::_type_info = {}; //LH
template<> IRNodeType ExprNode<Add>::_type_info = {};
template<> IRNodeType ExprNode<Sub>::_type_info = {};
template<> IRNodeType ExprNode<Mul>::_type_info = {};
template<> IRNodeType ExprNode<Div>::_type_info = {};
template<> IRNodeType ExprNode<HDiv>::_type_info = {}; //LH
template<> IRNodeType ExprNode<Mod>::_type_info = {};
template<> IRNodeType ExprNode<Min>::_type_info = {};
template<> IRNodeType ExprNode<Max>::_type_info = {};
template<> IRNodeType ExprNode<EQ>::_type_info = {};
template<> IRNodeType ExprNode<NE>::_type_info = {};
template<> IRNodeType ExprNode<LT>::_type_info = {};
template<> IRNodeType ExprNode<LE>::_type_info = {};
template<> IRNodeType ExprNode<GT>::_type_info = {};
template<> IRNodeType ExprNode<GE>::_type_info = {};
template<> IRNodeType ExprNode<And>::_type_info = {};
template<> IRNodeType ExprNode<Or>::_type_info = {};
template<> IRNodeType ExprNode<Not>::_type_info = {};
template<> IRNodeType ExprNode<Select>::_type_info = {};
template<> IRNodeType ExprNode<Load>::_type_info = {};
template<> IRNodeType ExprNode<Ramp>::_type_info = {};
template<> IRNodeType ExprNode<Broadcast>::_type_info = {};
template<> IRNodeType ExprNode<Call>::_type_info = {};
template<> IRNodeType ExprNode<Let>::_type_info = {};
template<> IRNodeType StmtNode<LetStmt>::_type_info = {};
template<> IRNodeType StmtNode<PrintStmt>::_type_info = {};
template<> IRNodeType StmtNode<AssertStmt>::_type_info = {};
template<> IRNodeType StmtNode<Pipeline>::_type_info = {};
template<> IRNodeType StmtNode<For>::_type_info = {};
template<> IRNodeType StmtNode<Store>::_type_info = {};
template<> IRNodeType StmtNode<Provide>::_type_info = {};
template<> IRNodeType StmtNode<Allocate>::_type_info = {};
template<> IRNodeType StmtNode<Realize>::_type_info = {};
template<> IRNodeType StmtNode<Block>::_type_info = {};

template<>
RefCount &ref_count<IRNode>(const IRNode *n) {return n->ref_count;}

template<>
void destroy<IRNode>(const IRNode *n) {delete n;}
}
}
