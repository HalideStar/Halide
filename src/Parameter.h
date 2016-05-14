#ifndef HALIDE_PARAMETER_H
#define HALIDE_PARAMETER_H

/** \file
 * Defines the internal representation of parameters to halide piplines
 */

#include "Expr.h"
#include "Buffer.h"

namespace Halide {
namespace Internal {

struct ParameterContents;

/** A reference-counted handle to a parameter to a halide
 * pipeline. May be a scalar parameter or a buffer */
class Parameter {
    IntrusivePtr<ParameterContents> contents;

    void check_defined() const;
    void check_is_buffer() const;
    void check_is_scalar() const;
    void check_dim_ok(int dim) const;

public:
    /** Construct a new undefined handle */
    EXPORT Parameter();

    /** Construct a new parameter of the given type. If the second
     * argument is true, this is a buffer parameter of the given
     * dimensionality, otherwise, it is a scalar parameter (and the
     * dimensionality should be zero). The parameter will be given a
     * unique auto-generated name. */
    EXPORT Parameter(Type t, bool is_buffer, int dimensions);

    /** Construct a new parameter of the given type with name given by
     * the third argument. If the second argument is true, this is a
     * buffer parameter, otherwise, it is a scalar parameter. The
     * third argument gives the dimensionality of the buffer
     * parameter. It should be zero for scalar parameters. If the
     * fifth argument is true, the the name being passed in was
     * explicitly specified (as opposed to autogenerated). If the
     * sixth argument is true, the Parameter is registered in the global
     * ObjectInstanceRegistry. */
    EXPORT Parameter(Type t, bool is_buffer, int dimensions,
                     const std::string &name, bool is_explicit_name = false,
                     bool register_instance = true);

    /** Copy ctor, operator=, and dtor, needed for ObjectRegistry accounting. */
    EXPORT Parameter(const Parameter&);
    EXPORT Parameter& operator=(const Parameter&);
    EXPORT ~Parameter();

    /** Get the type of this parameter */
    EXPORT Type type() const;

    /** Get the dimensionality of this parameter. Zero for scalars. */
    EXPORT int dimensions() const;

    /** Get the name of this parameter */
    EXPORT const std::string &name() const;

    /** Return true iff the name was explicitly specified */
    EXPORT bool is_explicit_name() const;

    /** Does this parameter refer to a buffer/image? */
    EXPORT bool is_buffer() const;

    /** If the parameter is a scalar parameter, get its currently
     * bound value. Only relevant when jitting */
    template<typename T>
    NO_INLINE T get_scalar() const {
        user_assert(type() == type_of<T>())
            << "Can't get Param<" << type()
            << "> as scalar of type " << type_of<T>() << "\n";
        return *((const T *)(get_scalar_address()));
    }

    /** This returns the current value of get_scalar<type()>()
     * as an Expr. */
    EXPORT Expr get_scalar_expr() const;

    /** If the parameter is a scalar parameter, set its current
     * value. Only relevant when jitting */
    template<typename T>
    NO_INLINE void set_scalar(T val) {
        user_assert(type() == type_of<T>())
            << "Can't set Param<" << type()
            << "> to scalar of type " << type_of<T>() << "\n";
        *((T *)(get_scalar_address())) = val;
    }

    /** If the parameter is a scalar parameter, get its default
     * value. */
    template<typename T>
    NO_INLINE T get_default() const {
        user_assert(type() == type_of<T>())
            << "Can't get default for Param<" << type()
            << "> to scalar of type " << type_of<T>() << "\n";
        return *((T *)(get_default_address()));
    }

    /** If the parameter is a scalar parameter, set its default
     * value. Useful when jitting and for introspection on code
     * written in Halide. */
    template<typename T>
    NO_INLINE T set_default(const T &val) const {
        user_assert(type() == type_of<T>())
            << "Can't set default for Param<" << type()
            << "> to scalar of type " << type_of<T>() << "\n";
        *((T *)(get_default_address())) = val;
    }

    /** If the parameter is a buffer parameter, get its currently
     * bound buffer. Only relevant when jitting */
    EXPORT Buffer get_buffer() const;

    /** If the parameter is a buffer parameter, set its current
     * value. Only relevant when jitting */
    EXPORT void set_buffer(Buffer b);

    /** Get the pointer to the current value of the scalar
     * parameter. For a given parameter, this address will never
     * change. Only relevant when jitting. */

    EXPORT void *get_scalar_address() const;
    EXPORT void *get_default_address() const;

    /** Tests if this handle is the same as another handle */
    EXPORT bool same_as(const Parameter &other) const;

    /** Tests if this handle is non-nullptr */
    EXPORT bool defined() const;

    /** Get and set constraints for the min, extent, and stride (see
     * ImageParam::set_extent) */
    //@{
    EXPORT void set_min_constraint(int dim, Expr e);
    EXPORT void set_extent_constraint(int dim, Expr e);
    EXPORT void set_stride_constraint(int dim, Expr e);
    EXPORT void set_host_alignment(int bytes);
    EXPORT Expr min_constraint(int dim) const;
    EXPORT Expr extent_constraint(int dim) const;
    EXPORT Expr stride_constraint(int dim) const;
    EXPORT int host_alignment() const;
    //@}

    /** Get and set constraints for scalar parameters. These are used
     * directly by Param, so they must be exported. */
    // @{
    EXPORT void set_min_value(Expr e);
    EXPORT Expr get_min_value() const;
    EXPORT void set_max_value(Expr e);
    EXPORT Expr get_max_value() const;
    // @}
};

/** Validate arguments to a call to a func, image or imageparam. */
void check_call_arg_types(const std::string &name, std::vector<Expr> *args, int dims);

}
}

#endif
