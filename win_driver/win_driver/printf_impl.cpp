#include "printf.hpp"

#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS 1

// Compile nanoprintf in this translation unit.
#define NANOPRINTF_IMPLEMENTATION
#include "nano_printf.h"

int snprintf(char* buffer, size_t bufsz, char const* fmt, va_list val) noexcept
{
  return npf_vsnprintf(buffer, bufsz, fmt, val);
}
