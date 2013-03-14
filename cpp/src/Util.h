// Always use assert, even if llvm-config defines NDEBUG
#ifdef NDEBUG
#undef NDEBUG
#include <assert.h>
#define NDEBUG
#else
#include <assert.h>
#endif

#ifndef HALIDE_UTIL_H
#define HALIDE_UTIL_H

/** \file
 * Various utility functions used internally Halide. */

#include <vector>
#include <string>

//LH
// We need cmath for the definition of hmod_imp
// If this is too untidy, we could make this a separate include file.
#include <cmath>
#include <stdio.h>

namespace Halide { 

//LH
// Implementations of operators that are not native to C, for use in optimisation and in testing.
// hmod is the Halide % operator.
template<typename T>
inline T hmod_imp(T a, T b)
{
    return ((long)(a % b) + b) % b;
}
// Special cases for float, double and unsigned int.
template<> inline float hmod_imp<float>(float a, float b) { return std::fmod(a,b); }
template<> inline double hmod_imp<double>(double a, double b) { return std::fmod(a,b); }
template<> inline unsigned int hmod_imp<unsigned int>(unsigned int a, unsigned int b) { return a % b; }
template<> inline unsigned short hmod_imp<unsigned short>(unsigned short a, unsigned short b) { return a % b; }
template<> inline unsigned char hmod_imp<unsigned char>(unsigned char a, unsigned char b) { return a % b; }

//LH
// hdiv is division defined so that the remainder is hmod.  The division is non-standard only for signed integers.
template<typename T> inline T hdiv_imp(T a, T b) { return a / b; }
template<> inline unsigned char hdiv_imp<unsigned char>(unsigned char a, unsigned char b) { return a / b; }
template<> inline unsigned short hdiv_imp<unsigned short>(unsigned short a, unsigned short b) { return a / b; }
template<> inline unsigned int hdiv_imp<unsigned int>(unsigned int a, unsigned int b) { return a / b; }
// Special cases for signed integers
template<> 
inline int hdiv_imp<int>(int a, int b) {
    long int longa;
    longa = ((long int) a) - hmod_imp(a, b); // Subtract the remainder to make an exact division.
    return (int) (longa / (long int) b);
}
template<> 
inline short hdiv_imp<short>(short a, short b) {
    long longa;
    longa = ((long) a) - hmod_imp(a, b); // Subtract the remainder to make an exact division.
    return (short) (longa / (long) b);
}
template<> 
inline char hdiv_imp<char>(char a, char b) {
    long longa;
    if (a == -128 && b == 127) { printf ("hmod(%d,%d) = %d\n", a, b, hmod_imp(a,b)); }
    longa = ((long) a) - hmod_imp(a, b); // Subtract the remainder to make an exact division.
    return (char) (longa / (long) b);
}



namespace Internal {

/** Build small vectors of up to 6 elements. If we used C++11 and
 * had vector initializers, this would not be necessary, but we
 * don't want to rely on C++11 support. */
//@{
template<typename T>
std::vector<T> vec(T a) {
    std::vector<T> v(1);
    v[0] = a;
    return v;
}

template<typename T>
std::vector<T> vec(T a, T b) {
    std::vector<T> v(2);
    v[0] = a;
    v[1] = b;
    return v;
}

template<typename T>
std::vector<T> vec(T a, T b, T c) {
    std::vector<T> v(3);
    v[0] = a;
    v[1] = b;
    v[2] = c;
    return v;
}

template<typename T>
std::vector<T> vec(T a, T b, T c, T d) {
    std::vector<T> v(4);
    v[0] = a;        
    v[1] = b;
    v[2] = c;
    v[3] = d;
    return v;
}

template<typename T>
std::vector<T> vec(T a, T b, T c, T d, T e) {
    std::vector<T> v(5);
    v[0] = a;        
    v[1] = b;
    v[2] = c;
    v[3] = d;
    v[4] = e;
    return v;
}

template<typename T>
std::vector<T> vec(T a, T b, T c, T d, T e, T f) {
    std::vector<T> v(6);
    v[0] = a;        
    v[1] = b;
    v[2] = c;
    v[3] = d;
    v[4] = e;
    v[5] = f;
    return v;
}


// Similar functions specifically for conveniently constructing vectors of strings from literals
std::vector<std::string> vecS(std::string a);
std::vector<std::string> vecS(std::string a, std::string b);
std::vector<std::string> vecS(std::string a, std::string b, std::string c);
std::vector<std::string> vecS(std::string a, std::string b, std::string c, std::string d);

// @}

/** Generate a unique name starting with the given character. It's
 * unique relative to all other calls to unique_name done by this
 * process. Not thread-safe. */
std::string unique_name(char prefix);

/** Test if the first string starts with the second string */
bool starts_with(const std::string &str, const std::string &prefix);

/** Test if the first string ends with the second string */
bool ends_with(const std::string &str, const std::string &suffix);

// LH: unique_name for programmer specified names.
// Programmer specified variable names are joined with function names during
// code generation so they do not need to be unique, but function names must be
// unique.  Library modules that create Halide code may be called multiple times,
// so the library-specified function names need to be unique.  This function
// accepts a string and makes a unique name out of it.
std::string unique_name(const std::string &name);
}
}

#endif
