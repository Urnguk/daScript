/*
 * Dagor Engine 5
 * Copyright (C) 2003-2019  Gaijin Entertainment Corp.  All rights reserved
 *
 * (for conditions of distribution and use, see License)
*/

#ifndef _DAGOR_PUBLIC_MATH_DAG_VECMATHDECL_H_
#define _DAGOR_PUBLIC_MATH_DAG_VECMATHDECL_H_
#pragma once

typedef const struct mat44f& mat44f_cref;
typedef const struct mat43f& mat43f_cref;
typedef const struct mat33f& mat33f_cref;
typedef const struct bbox3f& bbox3f_cref;
typedef const struct bsph3f& bsph3f_cref;

//either _TARGET_SIMD_SSE = 2,3,4 (SSE2, SSSE3, SSE4.1) or _TARGET_SIMD_NEON should be defined
//however, header will try to auto-detect target

#ifndef DECL_ALIGN16
  #if __cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1600)
    #define DECLSPEC_ALIGN16 alignas(16)
    #define ATTRIBUTE_ALIGN16
    #define DECL_ALIGN16(TYPE, VAR) alignas(16) TYPE VAR
  #else
    //pre c++11
    #ifdef _MSC_VER
      #define DECLSPEC_ALIGN16  __declspec(align(16))
      #define ATTRIBUTE_ALIGN16
    #else
      #define DECLSPEC_ALIGN16
      #define ATTRIBUTE_ALIGN16 __attribute__((aligned(16)))
    #endif
    #define DECL_ALIGN16(TYPE, VAR) DECLSPEC_ALIGN16 TYPE VAR ATTRIBUTE_ALIGN16
  #endif
#endif

#if defined(__SANITIZE_ADDRESS__)
# define DAGOR_ASAN_ENABLED
#elif defined(__has_feature)
# if __has_feature(address_sanitizer)
#   define DAGOR_ASAN_ENABLED
# endif
#endif

#ifndef VECMATH_FINLINE
  #define VECMATH_FINLINE __forceinline
#endif

#ifndef VECMATH_INLINE
  #define VECMATH_INLINE inline
#endif

#ifndef VECTORCALL
  //__vectorcall is faster on msvc, even on x64 target
  #if (defined(_MSC_VER) || defined(__clang__)) && __SSE__
    #define VECTORCALL __vectorcall
  #else
    #define VECTORCALL
  #endif
#endif

#if defined(DAGOR_ASAN_ENABLED) && (defined(__clang__) || __GNUC__ >= 7)
# define NO_ASAN_INLINE inline __attribute__((no_sanitize_address))
//loads have to switch off address sanitize. It is common (and ok) to load data from stack, even if data partially is not allocated (i.e. .xyzU).
#else
# define NO_ASAN_INLINE VECMATH_FINLINE
#endif

//if target is not defined, try to auto-detect target
#ifndef _TARGET_SIMD_SSE
  #if __SSE4_1__ || defined(__AVX__) || defined(__AVX2__)
    #define _TARGET_SIMD_SSE 4
  #elif __SSSE3__
    #define _TARGET_SIMD_SSE 3
  #elif defined(__SSE2__) || defined(_M_AMD64) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP>=1)
    #define _TARGET_SIMD_SSE 2
  #endif
#endif

#ifndef _TARGET_64BIT
  #if defined(_M_AMD64) || defined(_M_X64) || defined(_M_ARM64) || INTPTR_MAX == INT32_MAX
    #define _TARGET_64BIT 1
  #endif
#endif

#if !defined(_TARGET_SIMD_SSE) && !defined(_TARGET_SIMD_NEON)
  #if defined(__ARM_NEON) || defined(__ARM_NEON__)
    #define _TARGET_SIMD_NEON 1
  #endif
#endif

#if _TARGET_SIMD_SSE
  #include <emmintrin.h>

  typedef __m128 vec4f;
  typedef __m128 vec3f;
  typedef __m128i vec4i;

  typedef const vec4f vec4f_const;

  typedef const union alignas(16)
  {
    unsigned m128_u32[4];
    vec4i m128;
    operator vec4i() const { return m128; }
    operator vec4f() const { return (vec4f&)m128; }
  } vec4i_const;

#elif _TARGET_SIMD_NEON // PSP2, iOS
  #include <arm_neon.h>
  typedef float32x4_t vec4f;
  typedef float32x4_t vec3f;
  typedef int32x4_t   vec4i;
  typedef const vec4f vec4f_const;
  typedef const vec4i vec4i_const;

#else
 !error! unsupported target
#endif

//! Matrix 3x3, column major, 3x4 floats
struct mat33f
{
  vec3f col0, col1, col2;

  mat33f &operator =(mat33f_cref m) { col0 = m.col0; col1 = m.col1; col2 = m.col2; return *this;}
};

//! Matrix 4x4, column major, 4x4 floats
struct mat44f
{
  vec4f col0, col1, col2, col3;

  VECMATH_FINLINE mat44f VECTORCALL &operator =(mat44f_cref m)
  {
    col0 = m.col0;
    col1 = m.col1;
    col2 = m.col2;
    col3 = m.col3;
    return *this;
  }

  VECMATH_FINLINE void VECTORCALL set33(mat33f_cref m) { col0 = m.col0; col1 = m.col1; col2 = m.col2; }
  VECMATH_FINLINE void VECTORCALL set33(mat33f_cref m, vec4f c3)
    { col0 = m.col0; col1 = m.col1; col2 = m.col2; col3 = c3; }
};

union mat44f_const
{
  float f[16];
  vec4f col[4];
  VECMATH_FINLINE operator mat44f&() const { return *(mat44f*)this; }
};

//! Matrix 4x3, row major, 3x4 floats
struct mat43f
{
  vec4f row0, row1, row2;

  VECMATH_FINLINE mat43f VECTORCALL &operator =(mat43f_cref m)
    { row0 = m.row0; row1 = m.row1; row2 = m.row2; return *this;}
};

//! 3D bounding box
struct bbox3f
{
  vec4f bmin, bmax;

  VECMATH_FINLINE bbox3f VECTORCALL &operator =(bbox3f_cref b) { bmin = b.bmin; bmax = b.bmax; return *this; }
};

//! 3D bounding sphere (coded as .xyz=center, .w=radius)
struct bsph3f
{
  vec4f c_r;

  VECMATH_FINLINE bsph3f VECTORCALL &operator =(const bsph3f &b) { c_r = b.c_r; return *this; }
};

struct alignas(16) mat44f_align16: public mat44f
{
  VECMATH_FINLINE mat44f_align16 VECTORCALL &operator =(mat44f_cref m) {mat44f::operator = (m);return *this;}
};

struct alignas(16) mat43f_align16: public mat43f
{
  VECMATH_FINLINE mat43f_align16 VECTORCALL &operator =(mat43f_cref m) {mat43f::operator = (m);return *this;}
};

struct alignas(16) mat33f_align16: public mat33f
{
  VECMATH_FINLINE mat33f_align16 VECTORCALL &operator =(mat33f_cref m) {mat33f::operator = (m);return *this;}
};

struct alignas(16) bbox3f_align16: public bbox3f
{
  VECMATH_FINLINE bbox3f_align16 VECTORCALL &operator =(bbox3f_cref m) {bbox3f::operator = (m);return *this;}
};

struct alignas(16) bsph3f_align16: public bsph3f
{
  VECMATH_FINLINE bsph3f_align16 VECTORCALL &operator =(const bsph3f& m) {bsph3f::operator = (m);return *this;}
};
//! quaternion as 4D vector
typedef vec4f quat4f;

//! plane as 4D vector, .xyz=Norm, .w=D
typedef vec4f plane3f;

#endif
