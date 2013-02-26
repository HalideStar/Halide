#include "DomainInference.h"
#include "IR.h"
#include "IREquality.h"
#include "IROperator.h"
#include "IRVisitor.h"
#include "IRPrinter.h"
#include "log.h"
#include "Var.h"
#include "Simplify.h"

using std::string;

namespace Halide { 
namespace Internal {

// ForwardDomainInference walks the parse tree inferring forward domain bounds
// for functions.
// This operates on the very raw parse tree before functions are realized etc
// so that we can interpret the index expressions.

// Class ForwardDomainInference is only for internal use in this module.
class ForwardDomainInference : public IRVisitor {

private:
    using IRVisitor::visit;
    
    // The important methods are the visit methods that define handling of 
    // different tree nodes.
    
    // Call to a function, image buffer or image parameter.
    // e.g. g(x,y) = f(x,y-1);
    // From the expression y-1 we determine that the y bounds of g are
    // shifted compared to the y bounds of f.
    void visit (const Call *func_call)
    {
        // Arguments are in std::vector<Expr> func_call->args
        // If it is a call to another Halide function, Function func_call->func points to it
        // If it is a direct reference to an image, Buffer func_call->image is the buffer
        // and if it is an image parameter, Parameter func_call->param is that.
        // To check use ->func.value().defined(), ->image.defined() and ->param.defined().
        
        // Each of the argument expressions must be processed in turn.
        for (size_t i = 0; i < func_call->args.size(); i++)
        {
            // func_call->args[i] is the i'th dimension index expression for the call.
            // For now, just print it out.
            log(4,"LH") << "arg " << i << ": " << func_call->args[i] << '\n';
        }
    }
};

void domain_inference(Expr e)
{
    ForwardDomainInference infers;
    
    e.accept(&infers);
}


// BackwardIntervalInference walks an argument expression and
// determines the domain interval in the callee based on the
// domain interval in the caller, which is passed to it.

class BackwardIntervalInference : public IRVisitor {
public:
    Expr xmin;
    Expr xmax;
    std::string varname;
    bool poison;
    
    BackwardIntervalInference(Expr axmin, Expr axmax) : 
        xmin(axmin), xmax(axmax), varname(""), poison(false) {}

private:
    using IRVisitor::visit;
    
    void visit(const Variable *var)
    {
        // Variable node defines the varname string - the variable for which we are
        // building the inverse function.
        varname = var->name;
    }
    
    void visit(const Add *op)
    {
        if (is_const(op->b))
        {
            // Looking for constant on RHS of Add node.
            xmin = xmin - op->b;
            xmax = xmax - op->b;
            // Process the tree recursively
            op->a.accept(this);
        }
        else
        {
            assert(! is_const(op->a) && "Simplify did not put constant on RHS of Add");
            poison = true; // Expression cannot be solved as it has branches not simplified out.
        }
    }
};

/* Notes
Difference between Var and Variable.  Variable is a parse tree node.
Var is just a name.
*/

Interval backwards_interval(Expr e, Expr xmin, Expr xmax, std::string &v)
{
    BackwardIntervalInference infers(xmin, xmax);
    Expr e1 = simplify(e);
    
    e1.accept(&infers);
    
    Interval result(infers.xmin, infers.xmax);
    if (result.min.defined()) result.min = simplify(result.min);
    if (result.max.defined()) result.max = simplify(result.max);
    v = infers.varname;

    return result;
}

void check_interval(Expr e, Expr xmin, Expr xmax, 
                    Expr correct_min, Expr correct_max, std::string correct_varname)
{
    std::string v;
    Interval result = backwards_interval(e, xmin, xmax, v);
    
    Expr e1 = simplify(e); // Duplicate simplification for debugging only
    log(0,"LH") << "e: " << e << "    ";
    log(0,"LH") << "e1: " << e1 << '\n';
    //log(0,"LH") << "min: " << result.min << "    ";
    //log(0,"LH") << "max: " << result.max << '\n';
    
    log(0,"LH") << "min: " << result.min << "    ";
    log(0,"LH") << "max: " << result.max << '\n';
    
    bool success = true;
    if (!equal(result.min, correct_min)) {
        std::cout << "Incorrect min: " << result.min << std::endl
                  << "Should have been: " << correct_min << std::endl;
        success = false;
    }
    if (!equal(result.max, correct_max)) {
        std::cout << "Incorrect max: " << result.max << std::endl
                  << "Should have been: " << correct_max << std::endl;
        success = false;
    }
    assert(success && "Domain inference test failed");
}

void domain_inference_test()
{
    Var x("x");
    std::string v = "<dummy>";
    Expr cmin, cmax;
    
    // Tests of backward interval inference
    check_interval(x, 0, 100, 0, 100, "x");
    check_interval(x + 1, 0, 100, -1, 99, "x");
    check_interval(1 + x, 0, 100, -1, 99, "x");
    //check_interval(x + x, 0, 100, cmin, cmax, v);
    check_interval(2 * x, 0, 100, 0, 50, "x");
    check_interval(x * 2, 0, 100, 0, 50, "x");
    check_interval(x / 2, 0, 100, 0, 201, "x");
    //check_interval((x + 1) / 2, 0, 100, cmin, cmax, v);
    //check_interval((x + 2) / 2, 0, 100, cmin, cmax, v);
    //check_interval((2 * x + 4) / 2, 0, 100, cmin, cmax, v);
    std::cout << "Domain inference test passed" << std::endl;
    return;
}

}
}