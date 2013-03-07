#ifndef HALIDE_DOMAININFERENCE_H
#define HALIDE_DOMAININFERENCE_H

namespace Halide {
namespace Internal {
struct VarInterval
{
    // min and max are the range of the interval.
    // poison evaluates to true when the range is meaningless and, in fact, poisoned.
    // varname is the name of the variable described by the interval.
    Expr imin, imax, poison;
    std::string varname;
    
    VarInterval(std::string v, Expr poisoned, Expr emin, Expr emax) : 
        imin(emin), imax(emax), poison(poisoned), varname(v) {}
        
    VarInterval() : imin(Expr()), imax(Expr()), poison(Expr()), varname("") {}

    
    void update(VarInterval result);
};
}

struct Domain {
    std::vector<Halide::Internal::VarInterval> intervals;
    
    Domain();
    Domain(std::string xv, Expr xpoisoned, Expr xmin, Expr xmax);
    Domain(std::string xv, Expr xpoisoned, Expr xmin, Expr xmax, 
           std::string yv, Expr ypoisoned, Expr ymin, Expr ymax);
    Domain(std::string xv, Expr xpoisoned, Expr xmin, Expr xmax, 
           std::string yv, Expr ypoisoned, Expr ymin, Expr ymax, 
           std::string zv, Expr zpoisoned, Expr zmin, Expr zmax);
           
    int find(std::string v);
};

namespace Internal {
Domain domain_inference(std::vector<std::string> variables, Expr e);

void domain_inference_test();
}
}
#endif