#pragma once

// glibc math functions if you need some for your purposes

extern "C"
{
  double __ieee754_log(double x);
  double __ieee754_sqrt(double x);
  double __ieee754_pow(double x, double y);
  double __ieee754_ceil(double x);
};

extern "C" double log(double x);

extern "C" double sqrt(double x);

extern "C" double pow(double x, double y);

extern "C" double ceil(double x);
