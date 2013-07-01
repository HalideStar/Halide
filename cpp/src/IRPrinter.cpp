#include "IRPrinter.h"
#include "IROperator.h"
#include "IR.h"
#include "Func.h"
#include "DomainInference.h"

#include <iostream>
#include <sstream>

using std::ostream;
using std::endl;
using std::vector;
using std::string;
using std::ostringstream;

namespace std {
using Halide::operator<<;

// To print a std::vector, the method must be defined in namespace std.
ostream &operator<<(ostream &stream, const vector<Halide::Expr> &v) {
    for (size_t i = 0; i < v.size(); i++) {
        stream << v[i];
        if (i+1 < v.size()) {
            stream << ", ";
        }
    }
    return stream;
}

ostream &operator<<(ostream &stream, const vector<Halide::Interval> &v) {
    for (size_t i = 0; i < v.size(); i++) {
        stream << v[i];
        if (i+1 < v.size()) {
            stream << " ";
        }
    }
    return stream;
}

ostream &operator<<(ostream &stream, const vector<Halide::DomInterval> &v) {
    for (size_t i = 0; i < v.size(); i++) {
        stream << v[i];
        if (i+1 < v.size()) {
            stream << " ";
        }
    }
    return stream;
}
}

namespace Halide {

ostream &operator<<(ostream &out, Type type) {
    switch (type.t) {
    case Type::Int:
        out << 'i';
        break;
    case Type::UInt:
        out << 'u';
        break;
    case Type::Float:
        out << 'f';
        break;
    default:
        assert(false && "Malformed type");
    }
    out << type.bits;
    if (type.width > 1) out << 'x' << type.width;
    return out;
}

ostream &operator<<(ostream &stream, Expr ir) {
    if (!ir.defined()) {
        stream << "(undefined)";
    } else {
        Internal::IRPrinter p(stream);
        p.print(ir);
    }
    return stream;
}

ostream &operator<<(ostream &stream, Func f) {
    // Should also print the variables list
    stream << f.name() << " = ";
    if (!f.value().defined()) {
        stream << "(undefined)";
    } else {
        stream << f.value();
    }
    // Should also print the definition of the reduction function.
    return stream;
}


ostream &operator<<(ostream &stream, Domain d) {
    for (int i = 0; i < d.dimensions(); i++) {
        stream << d.intervals[i];
        if (i < d.dimensions()) stream << " ";
    }
    return stream;
}

ostream &operator<<(ostream &stream, Interval v) {
    stream << "interval";
    if (! v.exact) stream << "~"; // Approximate, not exact interval.
    stream << "(" << v.min << ", " << v.max << ")";
    return stream;
}

ostream &operator<<(ostream &stream, DomInterval v) {
    if (! v.exact) stream << "~"; // Approximate, not exact interval.
    stream << "[" << v.min << ", " << v.max << "]";
    return stream;
}

ostream &operator<<(ostream &stream, Range v) {
    stream << "range";
    if (! v.exact) stream << "~"; // Approximate, not exact range.
    stream << "(" << v.min << ", " << v.extent << ")";
    return stream;
}

namespace Internal {

void IRPrinter::test() {
    Type i32 = Int(32);
    Type f32 = Float(32);
    Expr x = new Variable(Int(32), "x");
    Expr y = new Variable(Int(32), "y");
    ostringstream expr_source;
    expr_source << (x + 3) * (y / 2 + 17);
    assert(expr_source.str() == "((x + 3)*((y/2) + 17))");

    Stmt store = new Store("buf", (x * 17) / (x - 3), y - 1);
    Stmt for_loop = new For("x", -2, y + 2, For::Parallel, store);
    vector<Expr> args(1); args[0] = x % 3;
    Expr call = new Call(i32, "buf", args);
    Stmt store2 = new Store("out", call + 1, x);
    Stmt for_loop2 = new For("x", 0, y, For::Vectorized , store2);
    Stmt pipeline = new Pipeline("buf", for_loop, Stmt(), for_loop2);
    Stmt assertion = new AssertStmt(y > 3, "y is greater than 3");
    Stmt block = new Block(assertion, pipeline);
    Stmt let_stmt = new LetStmt("y", 17, block);
    Stmt allocate = new Allocate("buf", f32, 1023, let_stmt);

    ostringstream source;
    source << allocate;
    std::string correct_source = \
        "allocate buf[f32 * 1023]\n"
        "let y = 17\n"
        "assert((y > 3), \"y is greater than 3\")\n"
        "produce buf {\n"
        "  parallel (x, -2, (y + 2)) {\n"
        "    buf[(y - 1)] = ((x*17)/(x - 3))\n"
        "  }\n"
        "} consume {\n"
        "  vectorized (x, 0, y) {\n"
        "    out[x] = (buf((x % 3)) + 1)\n"
        "  }\n"
        "}\n"
        "free buf\n";

    if (source.str() != correct_source) {
        std::cout << "Correct output:" << std::endl << correct_source;
        std::cout << "Actual output:" << std::endl << source.str();
        assert(false);
    }        
    std::cout << "IRPrinter test passed" << std::endl;
}

ostream &operator<<(ostream &out, For::ForType type) {
    switch (type) {
    case For::Serial:
        out << "for";
        break;
    case For::Parallel:
        out << "parallel";
        break;
    case For::Unrolled:
        out << "unrolled";
        break;
    case For::Vectorized:
        out << "vectorized";
        break;
    default:
        assert(false && "Malformed for type");
    }
    return out;
}

ostream &operator<<(ostream &out, const LoopSplitInfo &info) {
    bool gap = false;
    switch (info.status) {
    case LoopSplitInfo::Before:
        out << "before";
        gap = true;
        break;
    case LoopSplitInfo::Main:
        out << "main";
        gap = true;
        break;
    case LoopSplitInfo::After:
        out << "after";
        gap = true;
        break;
    case LoopSplitInfo::Ordinary:
        break;
    default:
        out << "<unknown status=" << info.status << ">";
        break;
    }
    if (info.status == LoopSplitInfo::Ordinary || info.status == LoopSplitInfo::Main) {
        if (info.defined()) {
            if (info.auto_split != LoopSplitInfo::Undefined) {
                if (gap) out << " ";
                switch (info.auto_split) {
                case LoopSplitInfo::Yes:
                    out << "auto";
                    break;
                case LoopSplitInfo::No:
                    out << "no_auto";
                    break;
                case LoopSplitInfo::Undefined:
                    break;
                default:
                    out << "<unknown auto_split=" << info.auto_split << ">";
                    break;
                }
                gap = true;
            }
            if (info.interval.min.defined() || info.interval.max.defined()) {
                if (gap) out << " ";
                out << info.interval;
            }
        }
    }
    return out;
}

ostream &operator<<(ostream &out, Clamp::ClampType type) {
    switch (type) {
    case Clamp::None:
        out << "clamp_none";
        break;
    case Clamp::Replicate:
        out << "clamp";
        break;
    case Clamp::Wrap:
        out << "wrap";
        break;
    case Clamp::Reflect:
        out << "reflect";
        break;
    case Clamp::Reflect101:
        out << "reflect101";
        break;
    case Clamp::Tile:
        out << "tile";
        break;
    default:
        assert(false && "Malformed clamp type");
    }
    return out;
}

ostream &operator<<(ostream &stream, Stmt ir) {
    if (!ir.defined()) {
        stream << "(undefined)" << std::endl;
    } else {
        Internal::IRPrinter p(stream);
        p.print(ir);
    }
    return stream;
}

IRPrinter::IRPrinter(ostream &s) : stream(s), indent(0) {
    s.setf(std::ios::fixed, std::ios::floatfield);    
}

void IRPrinter::print(Expr ir) {
    ir.accept(this);
}

void IRPrinter::print(Stmt ir) {
    ir.accept(this);
}


void IRPrinter::do_indent() {
    for (int i = 0; i < indent; i++) stream << ' ';
}
    
void IRPrinter::visit(const IntImm *op) {
    stream << op->value;
}
    
void IRPrinter::visit(const FloatImm *op) {
    stream << op->value << 'f';
}
    
void IRPrinter::visit(const Cast *op) { 
    stream << op->type << '(';
    print(op->value);
    stream << ')';
}
    
void IRPrinter::visit(const Variable *op) {
    // omit the type
    //stream << op->name << "." << op->type;
    stream << op->name;
}
    
//LH
void IRPrinter::visit(const BitAnd *op) {
    stream << '(';
    print(op->a);
    stream << " & ";
    print(op->b);
    stream << ')';
}

//LH
void IRPrinter::visit(const BitOr *op) {
    stream << '(';
    print(op->a);
    stream << " | ";
    print(op->b);
    stream << ')';
}

//LH
void IRPrinter::visit(const BitXor *op) {
    stream << '(';
    print(op->a);
    stream << " ^ ";
    print(op->b);
    stream << ')';
}

//LH
void IRPrinter::visit(const SignFill *op) {
    stream << "signfill(";
    print(op->value);
    stream << ')';
}

//LH
void IRPrinter::visit(const Clamp *op) {
    stream << "Clamp::" << op->clamptype <<"(";
    print(op->a);
    stream << ',';
    print(op->min);
    stream << ',';
    print(op->max);
	//if (op->clamptype == Clamp::Tile) {
		stream << ',';
		print(op->p1);
	//}
    stream << ')';
}

void IRPrinter::visit(const Add *op) {
    stream << '(';
    print(op->a);
    stream << " + ";
    print(op->b);
    stream << ')';
}

void IRPrinter::visit(const Sub *op) {
    stream << '(';
    print(op->a);
    stream << " - ";
    print(op->b);
    stream << ')';
}

void IRPrinter::visit(const Mul *op) {
    stream << '(';
    print(op->a);
    stream << "*";
    print(op->b);
    stream << ')';
}

void IRPrinter::visit(const Div *op) {
    stream << '(';
    print(op->a);
    stream << "/";
    print(op->b);
    stream << ')';
}

void IRPrinter::visit(const Mod *op) {
    stream << '(';
    print(op->a);
    stream << " % ";
    print(op->b);
    stream << ')';
}

void IRPrinter::visit(const Min *op) {
    stream << "min(";
    print(op->a);
    stream << ", ";
    print(op->b);
    stream << ")";
}

void IRPrinter::visit(const Max *op) {
    stream << "max(";
    print(op->a);
    stream << ", ";
    print(op->b);
    stream << ")";
}

void IRPrinter::visit(const EQ *op) {
    stream << '(';
    print(op->a);
    stream << " == ";
    print(op->b);
    stream << ')';
}

void IRPrinter::visit(const NE *op) {
    stream << '(';
    print(op->a);
    stream << " != ";
    print(op->b);
    stream << ')';
}

void IRPrinter::visit(const LT *op) {
    stream << '(';
    print(op->a);
    stream << " < ";
    print(op->b);
    stream << ')';
}

void IRPrinter::visit(const LE *op) {
    stream << '(';
    print(op->a);
    stream << " <= ";
    print(op->b);
    stream << ')';
}

void IRPrinter::visit(const GT *op) {
    stream << '(';
    print(op->a);
    stream << " > ";
    print(op->b);
    stream << ')';
}

void IRPrinter::visit(const GE *op) {
    stream << '(';
    print(op->a);
    stream << " >= ";
    print(op->b);
    stream << ')';
}

void IRPrinter::visit(const And *op) {
    stream << '(';
    print(op->a);
    stream << " && ";
    print(op->b);
    stream << ')';
}

void IRPrinter::visit(const Or *op) {
    stream << '(';
    print(op->a);
    stream << " || ";
    print(op->b);
    stream << ')';
}

void IRPrinter::visit(const Not *op) {
    stream << '!';
    print(op->a);
}

void IRPrinter::visit(const Select *op) {
    stream << "select(";
    print(op->condition);
    stream << ", ";
    print(op->true_value);
    stream << ", ";
    print(op->false_value);
    stream << ")";
}

void IRPrinter::visit(const Load *op) {
    stream << op->name << "[";
    print(op->index);
    stream << "]";
}

void IRPrinter::visit(const Ramp *op) {
    stream << "ramp(";
    print(op->base);
    stream << ", ";
    print(op->stride);
    stream << ", " << op->width << ")";
}

void IRPrinter::visit(const Broadcast *op) {
    stream << "x" << op->width << "(";
    print(op->value);
    stream << ")";
}

void IRPrinter::visit(const Call *op) {
    stream << op->name << "(";
    for (size_t i = 0; i < op->args.size(); i++) {
        print(op->args[i]);
        if (i < op->args.size() - 1) {
            stream << ", ";
        }
    }
    stream << ")";
}

void IRPrinter::visit(const Let *op) {
    stream << "(let " << op->name << " = ";
    print(op->value);
    stream << " in ";
    print(op->body);
    stream << ")";
}

void IRPrinter::visit(const LetStmt *op) {
    do_indent();
    stream << "let " << op->name << " = ";
    print(op->value);
    stream << endl;

    print(op->body);
}

void IRPrinter::visit(const PrintStmt *op) {
    do_indent();
    stream << "print(" << op->prefix;
    for (size_t i = 0; i < op->args.size(); i++) {
        stream << ", ";
        print(op->args[i]);
    }
    stream << ")" << endl;
}

void IRPrinter::visit(const AssertStmt *op) {
    do_indent();
    stream << "assert(";
    print(op->condition);
    stream << ", \"" << op->message << "\")" << endl;
}

void IRPrinter::visit(const Pipeline *op) {


    do_indent();
    stream << "produce " << op->name << " {" << endl;
    indent += 2;
    print(op->produce);
    indent -= 2;

    if (op->update.defined()) {
        do_indent();
        stream << "} update {" << endl;
        indent += 2;
        print(op->update);
        indent -= 2;
    }
        
    do_indent();
    stream << "} consume {" << endl;
    indent += 2;
    print(op->consume);
    indent -= 2;

    do_indent();
    stream << "}" << endl;
}

void IRPrinter::visit(const For *op) {

    do_indent();
    stream << op->for_type << " (" << op->name << ", ";
    print(op->min);
    stream << ", ";
    print(op->extent);
    if (op->loop_split.defined() || op->loop_split.status != LoopSplitInfo::Ordinary) {
        stream << ", " << op->loop_split;
    }
    stream << ") {" << endl;
        
    indent += 2;
    print(op->body);
    indent -= 2;

    do_indent();
    stream << "}" << endl;
}

void IRPrinter::visit(const Store *op) {
    do_indent();
    stream << op->name << "[";
    print(op->index);
    stream << "] = ";
    print(op->value);
    stream << endl;
}

void IRPrinter::visit(const Provide *op) {
    do_indent();
    stream << op->name << "(";
    for (size_t i = 0; i < op->args.size(); i++) {
        print(op->args[i]);
        if (i < op->args.size() - 1) stream << ", ";
    }
    stream << ") = ";
    print(op->value);
    stream << endl;
}

void IRPrinter::visit(const Allocate *op) {
    do_indent();
    stream << "allocate " << op->name << "[" << op->type << " * ";
    print(op->size);
    stream << "]" << endl;
    print(op->body);

    do_indent();
    stream << "free " << op->name << endl;
}

void IRPrinter::visit(const Realize *op) {
    do_indent();
    stream << "realize " << op->name << "(";
    for (size_t i = 0; i < op->bounds.size(); i++) {
        stream << "[";
        print(op->bounds[i].min);
        stream << ", ";
        print(op->bounds[i].extent);
        stream << "]";
        if (i < op->bounds.size() - 1) stream << ", ";
    }
    stream << ") {" << endl;

    indent += 2;
    print(op->body);
    indent -= 2;

    do_indent();
    stream << "}" << endl;
}

void IRPrinter::visit(const Block *op) {
    print(op->first);
    if (op->rest.defined()) print(op->rest);
}

void IRPrinter::visit(const Solve *op) {
    stream << "solve(";
    stream << op->v;
    stream << ": ";
    print(op->body);
    stream << ")";
}

void IRPrinter::visit(const TargetVar *op) {
    stream << "targetVar(" << op->name << ", ";
    print(op->body);
    stream << ")";
}

void IRPrinter::visit(const StmtTargetVar *op) {
    do_indent();
    stream << "stmtTargetVar(" << op->name << ") {" << endl;
    indent += 2;
    print(op->body);
    indent -= 2;
    
    do_indent();
    stream << "}" << endl;
}

void IRPrinter::visit(const Infinity *op) {
    if (op->count < 0) stream << "-";
    stream << "infinity";
    if (op->type.width > 1) stream << "_x" << op->type.width;
    if (op->count > 1) stream << "(" << op->count << ")";
    else if (op->count < -1) stream << "(" << -op->count << ")";
}
    
}}
