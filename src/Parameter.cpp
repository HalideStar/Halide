#include "IR.h"
#include "Parameter.h"

namespace Halide {
namespace Internal {
template<>
EXPORT RefCount &ref_count<Halide::Internal::ParameterContents>(const ParameterContents *p) {return p->ref_count;}

template<>
EXPORT void destroy<Halide::Internal::ParameterContents>(const ParameterContents *p) {delete p;}

//LH
/** Get an expression representing the extent of this image
 * parameter in the given dimension */
Expr Parameter::extent(int x) const {
    assert(is_buffer() && "Extent can only be obtained for a buffer (image) parameter");
    std::ostringstream s;
    s << name() << ".extent." << x;
    return Internal::Variable::make(Int(32), s.str(), *this);
}

//LH
/** Get an expression representing the min of this image
 * parameter in the given dimension */
Expr Parameter::min(int x) const {
    assert(is_buffer() && "Min can only be obtained for a buffer (image) parameter");
    std::ostringstream s;
    s << name() << ".min." << x;
    return Internal::Variable::make(Int(32), s.str(), *this);
}

}
}
