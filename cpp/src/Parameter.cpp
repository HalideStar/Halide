#include "IR.h"
#include "Parameter.h"

namespace Halide {
namespace Internal {
template<>
RefCount &ref_count<Halide::Internal::ParameterContents>(const ParameterContents *p) {return p->ref_count;}

template<>
void destroy<Halide::Internal::ParameterContents>(const ParameterContents *p) {delete p;}


/** Get an expression representing the extent of this image
 * parameter in the given dimension */
Expr Parameter::extent(int x) const {
    assert(is_buffer() && "Extent can only be obtained for a buffer (image) parameter");
    std::ostringstream s;
    s << name() << ".extent." << x;
    return new Internal::Variable(Int(32), s.str(), *this);
}

}
}
