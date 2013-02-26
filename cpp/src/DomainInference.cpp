#include "DomainInference.h"
#include "IR.h"
#include "IROperator.h"
#include "IRVisitor.h"
#include "IRPrinter.h"
#include "log.h"
#include "Var.h"

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
    // The important methods are the visit methods that define handling of 
    // different tree nodes.
    
    // Call to a function, image buffer or image parameter.
    // e.g. g(x,y) = f(x,y-1);
    // From the expression y-1 we determine that the y bounds of g are
    // shifted compared to the y bounds of f.
    virtual void visit (const Call *func_call)
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


/* Notes
Difference between Var and Variable.  Variable is a parse tree node.
Var is just a name.
*/

void infer_domain(Expr e, Expr xmin, Expr xmax, Expr &cmin, Expr &cmax, std::string &v)
{
    // Simplest assumption.  Just a single Var expression.
    cmin = xmin;
    cmax = xmax;
    const Variable *var_e = e.as<Variable>();
    if (var_e != NULL)
        v = var_e->name;
    return;
}

void domain_inference_test()
{
    Var x("x");
    std::string v = "<dummy>";
    Expr cmin, cmax;
    
    infer_domain(x, 0, 100, cmin, cmax, v);
    std::cout << "cmin: " << cmin << std::endl;
    std::cout << "cmax: " << cmax << std::endl;
    std::cout << "v: " << v << std::endl;
    std::cout << "Domain inference test passed" << std::endl;
    return;
}

}
}