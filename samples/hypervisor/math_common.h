#pragma once
#include <stdint.h>

#define __HI(x) *(1+(int*)&x)
#define __LO(x) *(int*)&x

int __ieee754_rem_pio2(double x, double* y);
double __ieee754_log(double x);
double __ieee754_sqrt(double x);
double __ieee754_pow(double x, double y);
double __ieee754_scalbn(double x, int n);
double __ieee754_ceil(double x);

int __kernel_rem_pio2(double* x, double* y, int e0, int nx, int prec, const int* ipio2);
double __kernel_cos(double x, double y);
double __kernel_sin(double x, double y, int iy);

double fabs(double x);
double copysign(double x, double y);
double floor(double x);
double cos(double x);
double sin(double x);

typedef union
{
  double value;
  struct
  {
    uint32_t lsw;
    uint32_t msw;
  } parts;
} ieee_double_shape_type;

#define INSERT_WORDS(d,ix0,ix1)         \
do {                \
  ieee_double_shape_type iw_u;          \
  iw_u.parts.msw = (ix0);         \
  iw_u.parts.lsw = (ix1);         \
  (d) = iw_u.value;           \
} while (0)

#define EXTRACT_WORDS(ix0,ix1,d)        \
do {                \
  ieee_double_shape_type ew_u;          \
  ew_u.value = (d);           \
  (ix0) = ew_u.parts.msw;         \
  (ix1) = ew_u.parts.lsw;         \
} while (0)

#define SET_LOW_WORD(d,v)         \
do {                \
  ieee_double_shape_type sl_u;          \
  sl_u.value = (d);           \
  sl_u.parts.lsw = (v);           \
  (d) = sl_u.value;           \
} while (0)

#define GET_HIGH_WORD(i,d)          \
do {                \
  ieee_double_shape_type gh_u;          \
  gh_u.value = (d);           \
  (i) = gh_u.parts.msw;           \
} while (0)

#define SET_HIGH_WORD(d,v)          \
do {                \
  ieee_double_shape_type sh_u;          \
  sh_u.value = (d);           \
  sh_u.parts.msw = (v);           \
  (d) = sh_u.value;           \
} while (0)
