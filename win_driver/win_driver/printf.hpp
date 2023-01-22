#pragma once
#include <cstdarg>

int snprintf(char* buffer, size_t bufsz, char const* fmt, va_list val) noexcept;
