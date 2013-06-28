// Include this file via IR.h
#include "IR.h"

#ifndef HALIDE_DOMAININFERENCE_H
#define HALIDE_DOMAININFERENCE_H

namespace Halide {
namespace Internal {
struct DomInterval
{
    InfInterval interval; // The interval bounds, possibly with infinity in them
    bool exact; // True if the interval is exact; set to false if an equation cannot be solved.
    
    DomInterval(InfInterval _interval, bool _exact = true) : interval(_interval), exact(_exact) {}
    DomInterval(Expr min, Expr max, bool _exact = true) : interval(InfInterval(Int(32), min, max)), exact(_exact) {}
    DomInterval() : interval(InfInterval()), exact(true) {}
    
    // Intersect the interval with another interval; AND the exact flags together.
    void intersect(DomInterval result);
};

/** Emit a DomInterval on an output stream (such as std::cout) in a
 * human-readable form */
std::ostream &operator<<(std::ostream &stream, DomInterval);

}

# define NextDomainType(dt) ((int) dt)++)

struct Domain {
    // Enumeration of different types of domain used in domain inference.
    // MaxDomains is always last in the list - it is the dimension required for an array.
    // Valid must be the first entry and must have value 0 because it is used as a loop initialiser.
    // None of the others may have defined values because we loop over the enum.
    typedef enum {Valid = 0, Computable, MaxDomains} DomainType;

    std::vector<Halide::Internal::DomInterval> intervals;

private:
    bool domain_locked;
public:

    EXPORT bool is_locked() { return domain_locked; }
    EXPORT void lock() { domain_locked = true; }
    
    Domain();
    Domain(Expr xmin, Expr xmax);
    Domain(Expr xmin, Expr xmax, Expr ymin, Expr ymax);
    Domain(Expr xmin, Expr xmax, Expr ymin, Expr ymax, Expr zmin, Expr zmax);
    Domain(Expr xmin, Expr xmax, Expr ymin, Expr ymax, Expr zmin, Expr zmax, Expr wmin, Expr wmax);
    
    Domain(InfInterval xint);
    Domain(InfInterval xint, InfInterval yint);
    Domain(InfInterval xint, InfInterval yint, InfInterval zint);
    Domain(InfInterval xint, InfInterval yint, InfInterval zint,
           InfInterval wint);
           
    Domain intersection(const Domain other) const;
    
    /** Constructor for an infinite domain of specified dimensionality */
    EXPORT static Domain infinite(int dimensions);
    
    // Accessors to read information out of the domain using dimension index number.
    EXPORT const Expr min(int index) const;
    EXPORT const Expr max(int index) const;
    EXPORT const bool exact(int index) const;
    EXPORT const Expr extent(int index) const;
    
    // Accessors to read the domain as native C data.
    EXPORT const int imin(int index) const;
    EXPORT const int imax(int index) const;
    EXPORT const int iextent(int index) const;
    
    EXPORT const int dimensions() const { return intervals.size(); }
};

namespace Internal {
std::vector<Domain> domain_inference(const std::vector<std::string> &variables, Expr e);

void domain_inference_test();
}
}
#endif