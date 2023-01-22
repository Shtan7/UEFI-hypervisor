#pragma once

extern "C"
{
#include <Uefi.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include "drvproto.h"
}

// Don't delete this statement otherwise you will get a lot of build errors.
#undef _DEBUG

inline const UINT16* operator""_w(const wchar_t* str, size_t)
{
  return reinterpret_cast<const UINT16*>(str);
}

inline const short* operator""_c(const char* str, size_t)
{
  return reinterpret_cast<const short*>(str);
}

inline void dead_loop()
{
  volatile int i = 0;

  while (i != 1)
  {

  }
}

template<class std_wstr>
UINTN Print(const std_wstr& str)
{
  if (str.data() != nullptr)
  {
    return Print(reinterpret_cast<const UINT16*>(str.data()));
  }

  return 0;
}
