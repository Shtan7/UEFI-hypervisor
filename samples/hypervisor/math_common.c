#include "math_common.h"

double fabs(double x)
{
  __HI(x) &= 0x7fffffff;
  return x;
}

double log(double x)
{
  return __ieee754_log(x);
}

double sqrt(double x)
{
  return __ieee754_sqrt(x);
}

double pow(double x, double y)
{
  return __ieee754_pow(x, y);
}

double ceil(double x)
{
  return __ieee754_ceil(x);
}

double copysign(double x, double y)
{
  __HI(x) = (__HI(x)&0x7fffffff)|(__HI(y)&0x80000000);
  return x;
}
