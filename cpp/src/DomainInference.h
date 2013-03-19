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
        imin(emin), imax(emax), poison(poisoned), varname(v) { }
        
    VarInterval() : imin(Expr()), imax(Expr()), poison(Expr()), varname("") {}

    
    void update(VarInterval result);
};
}

# define NextDomainType(dt) ((int) dt)++)

struct Domain {
    // Enumeration of different types of domain used in domain inference.
    // MaxDomains is always last in the list - it is the dimension required for an array.
    // Valid must be the first entry and must have value 0 because it is used as a loop initialiser.
    // None of the others may have defined values because we loop over the enum.
    typedef enum {Valid = 0, Computable, Efficient, MaxDomains} DomainType;

    std::vector<Halide::Internal::VarInterval> intervals;
    
    Domain();
    Domain(std::string xv, Expr xpoisoned, Expr xmin, Expr xmax);
    Domain(std::string xv, Expr xpoisoned, Expr xmin, Expr xmax, 
           std::string yv, Expr ypoisoned, Expr ymin, Expr ymax);
    Domain(std::string xv, Expr xpoisoned, Expr xmin, Expr xmax, 
           std::string yv, Expr ypoisoned, Expr ymin, Expr ymax, 
           std::string zv, Expr zpoisoned, Expr zmin, Expr zmax);
    Domain(std::string xv, Expr xpoisoned, Expr xmin, Expr xmax, 
           std::string yv, Expr ypoisoned, Expr ymin, Expr ymax, 
           std::string zv, Expr zpoisoned, Expr zmin, Expr zmax,
           std::string wv, Expr wpoisoned, Expr wmin, Expr wmax);
           
    // Equivalent constructors using C++ bool type for poisoned.
    // This approach is adopted as a hack because header file loops
    // make it impossible to include IROperator.h in Image.h
    Domain(std::string xv, bool xpoisoned, Expr xmin, Expr xmax);
    Domain(std::string xv, bool xpoisoned, Expr xmin, Expr xmax, 
           std::string yv, bool ypoisoned, Expr ymin, Expr ymax);
    Domain(std::string xv, bool xpoisoned, Expr xmin, Expr xmax, 
           std::string yv, bool ypoisoned, Expr ymin, Expr ymax, 
           std::string zv, bool zpoisoned, Expr zmin, Expr zmax);
    Domain(std::string xv, bool xpoisoned, Expr xmin, Expr xmax, 
           std::string yv, bool ypoisoned, Expr ymin, Expr ymax, 
           std::string zv, bool zpoisoned, Expr zmin, Expr zmax,
           std::string wv, bool wpoisoned, Expr wmin, Expr wmax);
           
    Domain intersection(Domain other);
    
    // Accessors to read information out of the domain using index number.
    const Expr min(int index) const;
    const Expr max(int index) const;
    const Expr exact(int index) const;
};

namespace Internal {
std::vector<Domain> domain_inference(const std::vector<std::string> &variables, Expr e);

void domain_inference_test();
}
}
#endif